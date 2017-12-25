#include "Arduino.h"
#include "VProtocolSenderHelper.h"

VProtocolSenderHelper::VProtocolSenderHelper(VProtocol_DataSender* radio, VProtocol_SenderHelperCallback* callback) {
  this->radio = radio;
  this->callback = callback;
  this->reset();
}

void VProtocolSenderHelper::reset() {
  this->counter = 0;
  this->retryAttempt = 0;
  
  this->resetBufDataLatched();
  this->resetBufDataPeeked();
  this->resetBufNonTxDataPeeked();
  this->resetBufTransmitData();
}

void VProtocolSenderHelper::resetBufDataLatched() {
  for (uint8_t i = 0; i < sizeof(this->dataLatched); i++) {
    this->dataLatched[i] = 0;
  }
}
void VProtocolSenderHelper::resetBufDataPeeked() {
  for (uint8_t i = 0; i < sizeof(this->dataPeeked); i++) {
    this->dataPeeked[i] = 0;
  }
}
void VProtocolSenderHelper::resetBufNonTxDataPeeked() {
  for (uint8_t i = 0; i < sizeof(this->dataNonTxPeeked); i++) {
    this->dataNonTxPeeked[i] = 0;
  }
}
void VProtocolSenderHelper::resetBufTransmitData() {
  for (uint8_t i = 0; i < sizeof(this->transmitData); i++) {
    this->transmitData[i] = 0;
  }
}

bool VProtocolSenderHelper::sendPacket() {
  if (this->retryAttempt == 0) {
    this->resetBufDataLatched();
    callback->latchData(&this->dataLatched[0]);
  } else {
    this->resetBufDataPeeked();
    callback->peekData(&this->dataPeeked[0]);
  }
  this->resetBufNonTxDataPeeked();
  callback->peekNonTransactionalData(&this->dataNonTxPeeked[0]);
  
  // prepare data to be sent
  this->resetBufTransmitData();
  this->transmitData[0] = 0x10; // protocol version
  this->transmitData[1] = this->counter;
  this->transmitData[2] = this->retryAttempt;
  this->transmitData[3] = this->mcuRestartFlag;
  for (uint8_t i = 0; i < sizeof(this->dataNonTxPeeked); i++) {
    this->transmitData[4+i] = this->dataNonTxPeeked[i];
  }
  for (uint8_t i = 0; i < sizeof(this->dataLatched); i++) {
    this->transmitData[10+i] = this->dataLatched[i];
  }
  for (uint8_t i = 0; i < sizeof(this->dataPeeked); i++) {
    this->transmitData[20+i] = this->dataPeeked[i];
  }
  
  bool res = false;
  radio->sendData(&this->transmitData[0], res);
  if (res) {
    this->counter++;
    this->retryAttempt = 0;
	this->mcuRestartFlag = 0;
  } else {
    this->retryAttempt++;
    if (this->retryAttempt > 200) {
      this->retryAttempt = 200;
    }
  }
  return res;
}