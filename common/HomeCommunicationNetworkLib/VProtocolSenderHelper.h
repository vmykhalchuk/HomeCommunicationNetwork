#ifndef VProtocolSenderHelper_h
#define VProtocolSenderHelper_h

// Read VProtocolHelpers.txt

#include "Arduino.h"

class VProtocol_SenderHelperCallback
{
  public:
    // Latch data from changes (user logic should start new data accumulation)
    // dataBuf is an array of 10 bytes
    virtual void latchData(uint8_t* dataBuf) = 0;
    // Peek data which was already accumulated to this point, user logic should maintain accumulating data, till latchData() is not called)
    // dataBuf is an array of 10 bytes
    virtual void peekData(uint8_t* dataBuf) = 0;
	// Peek Non-Transactional data, something which is not critical to guarantee delivery of all previous states, and most recent state is just fine to deliver
	// for example it could be 
	//		- ID of device - which obviously doesn't change
	//		- battery level - only most recent value is of our interest
	// dataBuf is an array of 6 bytes
	virtual void peekNonTransactionalData(uint8_t* dataBuf) = 0;
};

class VProtocol_DataSender
{
  public:
    // call to send data, true if data transmitted successfully, false otherwise
    virtual void sendData(uint8_t* data, bool& res) = 0;
};

class VProtocolSenderHelper
{
  public:
    VProtocolSenderHelper(VProtocol_DataSender* radio, VProtocol_SenderHelperCallback* callback);
    void reset();
    bool sendPacket();
  private:
    VProtocol_DataSender* radio;
    VProtocol_SenderHelperCallback* callback;
    uint8_t counter = 0;
	uint8_t retryAttempt = 0;
	uint8_t mcuRestartFlag = 1;
	
	uint8_t dataLatched[10];
	uint8_t dataPeeked[10];
	uint8_t dataNonTxPeeked[6];
	uint8_t transmitData[32];
	void resetBufDataLatched();
	void resetBufDataPeeked();
	void resetBufNonTxDataPeeked();
	void resetBufTransmitData();
};


#endif