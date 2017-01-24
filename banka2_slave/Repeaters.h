#ifndef Repeaters_h
#define Repeaters_h

#include "Arduino.h"
#include "Common.h"


/**
 * Hope you read this to understand how we select Repeater to send message to.
 * 
 * First of all we collect statistics.
 * However main tactics is to deliver message asap (if Master fails twice, then we switch to Relay 1 or 2, and if it fails, we switch to another relay)
 * However by switching to another relay - we also care to switch to the one we tried less - so we get statistics evenly distributed to make some judgment.
 * Also periodically we try to switch to most reliable channel - by looking through whole statistics and selecting the winner
 * 
 * FIXME Whats still is nto implemented in this repeater - is that we do not send periodically test messages to improve statistics, which we should
 */

class RepeatersProcessor_TestStub;
    //static const byte NODES_COUNT = 3; // 0 - Master; 1 - Repeater_1; 2 - Repeater_2; ...
    //static const uint16_t TIMER_SLOT_INTERVAL = (60/TICK_SECONDS)*30; // 30min
class RepeatersProcessor
{
  public:
    friend RepeatersProcessor_TestStub;
  protected:
    static const byte NODES_COUNT = 3; // 0 - Master; 1 - Repeater_1; 2 - Repeater_2; ...
    static const byte SLOTS_COUNT = 15;
    static const uint16_t TIMER_SLOT_INTERVAL = (60/TICK_SECONDS)*30; // 30min
    static const byte TIMER_SEND_TEST_MESSAGE_INTERVAL = 90 / TICK_SECONDS; // 90sec
    static const uint16_t TIMER_SEARCH_FOR_NEW_PERM_CHANNEL = (60/TICK_SECONDS)*60*3; // time used to search for next permanent channel (3 hours)
  private:
    uint16_t nextSlotTimer = TIMER_SLOT_INTERVAL; // every TICK_SECONDS(4) seconds it will be increased, when reaching TIMER_SLOT_INTERVAL it should switch to next statistics time slot
    uint8_t transmissionStatusPos = 0; // current time slot
    uint16_t transmissionStatusSuccesses[NODES_COUNT][SLOTS_COUNT];
    uint16_t transmissionStatusFailures[NODES_COUNT][SLOTS_COUNT];

    void cleanCurrentStatisticsSlot(); // will clean statistics' current time slot
    void updateStatistics(byte channel, boolean txSucceeded); // will update statistics' current time slot
    uint32_t sumSucc[NODES_COUNT];
    uint32_t sumFail[NODES_COUNT];
    void calculateStatisticsSummaries(byte slotsCount); // will put slotsCount starting from current and going back, and will summ into sumSucc[] and sumFail[] arrays - for every Node
    float calculatedStats[NODES_COUNT];
    void calculateStatistics(byte slotsCount);
    byte nextSlotIdx(byte slotIdx);

    // Channel: 0 - Main; 1 - Repeater 1; 2 - Repater 2; ...
    byte permanentSendChannel = 0; // one determined by statistics to be most reliable
    byte currentSendChannel = 0; // this channel is currently used - when failure occured - we try other channels temporarily
    byte currentSendChannelFailureNo = 0;
    byte currentSendChannelReoptimizeCounter = 0; // this counter is used to find more optimal channel if available (once a while we perform a statistics revalidation)

    byte registerSendStatusMode = 0; // 0 - ZERO; 1 - FOLLOWING_RESOLVED_LIST;
    byte registerSendStatusChannels[NODES_COUNT];
    byte registerSendStatusChannelsIdx = 0;
    void populateRegisterSendStatusChannels();

    byte getAnotherChannel(); // returns channel to switch to based on statistics (selects less used channel to equalize statistics)
    byte getMostOptimalChannel(byte slotsCount); // returns most optimal (from statistics point of view) channel; use 0xFF if want all slots to be counted
    
    /*
     * This section is for search of permanent channel
     * When decision is made to switch permanent channel - we start checking all channels to gather statistics, and when statistics are finally gathered - we decide on new permanent channel
     */
    boolean searchForNewPermChannel = true;
    uint16_t searchForNewPermChannelTimer = TIMER_SEARCH_FOR_NEW_PERM_CHANNEL;
    byte sendTestMessageTimer = TIMER_SEND_TEST_MESSAGE_INTERVAL;
    byte testChannel = 0; // channel where to send test message in search of permanent channel
  public:
    RepeatersProcessor();
    void every4SecTick();

    /**
     * Channel to use for sending: 0 - Main; 1 - Repeater 1; 2 - Repater 2; ...
     * NOTE: Always call registerSendStatus() after calling this method!
     */
    byte getChannel();
    /**
     * This must be called right after calling getChannel - and should represent status of getChannel transmit operation!
     */
    void registerSendStatus(boolean txSucceeded);
    byte getTestChannel(); // will return test channel to which to send (0xFF if no need to send)
    void registerTestMessageSendStatus(boolean txSucceeded);
};

class RepeatersProcessor_TestStub
{
  public:
    static void test(RepeatersProcessor& repeatersProcessor);
};

#endif
