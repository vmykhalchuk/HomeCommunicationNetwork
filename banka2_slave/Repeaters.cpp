#include "Arduino.h"
#include "Repeaters.h"

RepeatersProcessor::RepeatersProcessor()
{
  for (byte i = 0; i < NODES_COUNT; i++)
  {
    for (byte j = 0; j < SLOTS_COUNT; j++)
    {
      transmissionStatusSuccesses[i][j] = 0;
      transmissionStatusFailures[i][j] = 0;
    }
  }
}

void RepeatersProcessor::cleanCurrentStatisticsSlot()
{
  for (byte i = 0; i < NODES_COUNT; i++)
  {
    transmissionStatusSuccesses[i][transmissionStatusPos] = 0;
    transmissionStatusFailures[i][transmissionStatusPos] = 0;
  }
}

void RepeatersProcessor::updateStatistics(byte channel, boolean txSucceeded)
{
  if (txSucceeded)
  {
    transmissionStatusSuccesses[channel][transmissionStatusPos]++;
  }
  else
  {
    transmissionStatusFailures[channel][transmissionStatusPos]++;
  }
}

void RepeatersProcessor::calculateStatisticsSummaries(byte slotsCount)
{
  for (byte c = 0; c < NODES_COUNT; c++)
  {
    sumSucc[c] = 0;
    sumFail[c] = 0;
    byte slotIdx = transmissionStatusPos;
    for (byte j = 0; j < slotsCount; j++)
    {
      sumSucc[c] += transmissionStatusSuccesses[c][slotIdx];
      sumFail[c] += transmissionStatusFailures[c][slotIdx];
      
      slotIdx = nextSlotIdx(slotIdx);
    }
  }
}

void RepeatersProcessor::calculateStatistics(byte slotsCount)
{
  float slotCoefStep = (80 / SLOTS_COUNT); // for how much every consecutive slot coeficcient will decrease
  for (byte c = 0; c < NODES_COUNT; c++)
  {
    calculatedStats[c] = 0;
    byte slotIdx = transmissionStatusPos;
    for (byte j = 0; j < slotsCount; j++)
    {
      float slotCoef = 100 - slotCoefStep * j;
      uint16_t sum = transmissionStatusSuccesses[c][slotIdx] + transmissionStatusFailures[c][slotIdx];
      float v1 = (sum <= 5) ? 50 : (float (transmissionStatusSuccesses[c][slotIdx] * 100) / sum);
      v1 = v1 * 100 / slotCoef;
      calculatedStats[c] += v1;
      
      slotIdx = nextSlotIdx(slotIdx);
    }
  }
}

byte RepeatersProcessor::nextSlotIdx(byte slotIdx)
{
  if (slotIdx == 0)
  {
    slotIdx = SLOTS_COUNT - 1;
  }
  else
  {
    slotIdx--;
  }
  return slotIdx;
}

void RepeatersProcessor::every4SecTick()
{
  if (--nextSlotTimer == 0)
  {
    transmissionStatusPos++;
    transmissionStatusPos %= SLOTS_COUNT;
    cleanCurrentStatisticsSlot();
    nextSlotTimer = TIMER_SLOT_INTERVAL; // rewind timer
  }

  if (searchForNewPermChannelTimer>0) searchForNewPermChannelTimer--;
  if (sendTestMessageTimer>0) sendTestMessageTimer--;
}

byte RepeatersProcessor::getChannel()
{
  return currentSendChannel;
}

void RepeatersProcessor::registerSendStatus(boolean txSucceeded)
{
  updateStatistics(currentSendChannel, txSucceeded);
  
  if (txSucceeded)
  {
    registerSendStatusMode = 0;
    currentSendChannelFailureNo = 0;
    if (++currentSendChannelReoptimizeCounter > 50)
    { // once in a while we have to review our statistics to select most reliable channel
      currentSendChannelReoptimizeCounter = 0;
      currentSendChannel = getMostOptimalChannel(-1);
    }
    //currentSendChannel = permanentSendChannel; // lets stick to same channel - to avoid new searches again; still it is bad practice because this way we can stick to some bad channel with low performance
  }
  else
  {
    if (registerSendStatusMode == 0)
    {
      if (++currentSendChannelFailureNo >= 2) // after two consecutive failure attempts we switch to another channel
      {
        currentSendChannelFailureNo = 0;
        
        populateRegisterSendStatusChannels();
        registerSendStatusChannelsIdx = 0;
        currentSendChannel = registerSendStatusChannels[registerSendStatusChannelsIdx];

        registerSendStatusMode = 1;
      }
    }
    else if (registerSendStatusMode == 1)
    {
      if (++currentSendChannelFailureNo >= 2) // after two consecutive failure attempts we switch to another channel
      {
        currentSendChannelFailureNo = 0;
        
        registerSendStatusChannelsIdx++;
        currentSendChannel = registerSendStatusChannels[registerSendStatusChannelsIdx];

        if (registerSendStatusChannelsIdx == (NODES_COUNT - 1))
        {
          registerSendStatusMode = 0;
        }
      }
    }
  }
}

void RepeatersProcessor::populateRegisterSendStatusChannels()
{
  calculateStatistics(NODES_COUNT);
  // populate registerSendStatusChannels[] by storing node with highest stats first and lowest last
  for (byte i = 0; i < NODES_COUNT; i++)
  {
    float max = -1;
    byte maxC = 0xFF;
    for (byte j = 0; j < NODES_COUNT; j++)
    {
      if (calculatedStats[i] > (max + 0.01))
      {
        max = calculatedStats[i];
        maxC = i;
      }
    }
    registerSendStatusChannels[i] = maxC;
    calculatedStats[maxC] = -1;
  }
}

byte RepeatersProcessor::getAnotherChannel()
{
  // select next testChannel (with lowest statistics collected)
  calculateStatisticsSummaries(3); // we load only recent statistics to have shorter action time and not remember whats happened long ago
  
  uint32_t _min = 0xFFFFFFFF;
  byte _minC = 0xFF;
  for (byte c = 0; c < NODES_COUNT; c++)
  { // priority is given to M (channel#0)
    uint32_t sum = sumSucc[c] + sumFail[c];

    // mechanism to prevent switching to mostly failling channel
    if (sum > 5 && sumFail[c] > sumSucc[c]*3) {
      continue; // skip this one cause its failling too often
    }
    
    if (sum < _min)
    {
      _min = sum;
      _minC = c;
    }
  }

  if (_minC == 0xFF)
  { // if all are failing much, still have to select one
    for (byte c = 0; c < NODES_COUNT; c++)
    { // priority is given to M (channel#0)
      uint32_t sum = sumSucc[c] + sumFail[c];
      if (sum < _min)
      {
        _min = sum;
        _minC = c;
      }
    }
  }
  
  return _minC;
}

byte RepeatersProcessor::getMostOptimalChannel(byte slotsCount)
{ // FIXME we should rate info from older slots less valuable
  calculateStatisticsSummaries((slotsCount < 1 || slotsCount > SLOTS_COUNT) ? SLOTS_COUNT: slotsCount);

  byte highestSuccRateC = 0xFF;
  float highestSuccRate = -1;
  for (byte c = 0; c < NODES_COUNT; c++)
  {
    uint32_t sum = sumSucc[c] + sumFail[c];
    float succRate = (sum <= 5) ? 50 : (float (sumSucc[c] * 100) / sum);
    if (succRate > (highestSuccRate+0.1))
    {
      highestSuccRate = succRate;
      highestSuccRateC = c;
    }
  }
  return highestSuccRateC;
}

byte RepeatersProcessor::getTestChannel()
{
  if (true) return 0xFF; // disable this functionality for a moment
  if (searchForNewPermChannel && sendTestMessageTimer == 0)
  {
    sendTestMessageTimer = TIMER_SEND_TEST_MESSAGE_INTERVAL;
    return testChannel;
  }
  else
  {
    return 0xFF;
  }
}

void RepeatersProcessor::registerTestMessageSendStatus(boolean txSucceeded)
{
  updateStatistics(testChannel, txSucceeded);
  // select next testChannel (with lowest statistics collected)
  testChannel = getAnotherChannel();
}

void RepeatersProcessor_TestStub::test(RepeatersProcessor& proc)
{
  _debugln("Tests initializing...");

  unsigned long t = millis();
  for (int i = 0; i < 1550; i++)
  {
    byte ch = proc.getChannel();
    proc.registerSendStatus(i % 13 == 0);
    if (i % 100 == 0)
    {
      for (int j = 0; j < RepeatersProcessor::TIMER_SLOT_INTERVAL+2; j++)
      {
        proc.every4SecTick();
      }
    }
  }
  _debug("Init has taken: "); _debug(millis() - t); _debugln("ms");

  _debug("Stats:");
  for (byte c = 0; c < RepeatersProcessor::NODES_COUNT; c++)
  {
    for (byte z = 0; z < 2; z++)
    {
      _debug(z == 0 ? "Successes" : "Failures"); _debug(" for Node: "); _debugln(c);
      byte s = proc.transmissionStatusPos;
      for (int i = 0; i < RepeatersProcessor::SLOTS_COUNT; i++)
      {
        uint16_t data = z == 0 ? proc.transmissionStatusSuccesses[c][s] : proc.transmissionStatusFailures[c][s];
        _debug(1000 + data); _debug(", ");
        s = proc.nextSlotIdx(s);
      }
      _debugln();
    }
  }

  for (int i = 0; i < 20; i++)
  {
    byte ch = proc.getChannel();
    _debug(ch);
    unsigned long t = micros();
    proc.registerSendStatus(i % 23 == 0);
    t = micros() - t;
    _debug(" - "); _debugln(t);
  }
  
}

