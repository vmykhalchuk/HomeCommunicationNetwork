#include "Arduino.h"
#include "GSMUtils.h"

#define LOG (*logComm)
#define GSMSerial (*gsmComm)

GSMUtils::GSMUtils(Stream* gsmComm, Stream* logComm)
{
  this->gsmComm = gsmComm;
  this->logComm = logComm;
}

void GSMUtils::_serialEvent()
{
  if (gsm_rx_buf_full_line) return; // we have to wait before line is processed

  if (gsm_rx_buf_line_cr && (z++ > 500)) // TODO set it to 1000, or tune it accordingly to speed of COM port
  {
    LOG.print('!');
    if (gsm_rx_buf_line_cr) {
      gsm_rx_buf_full_line = true;
      return;
    }
  }
  
  while (GSMSerial.available())
  {
    z = 0;
    char c = (char)GSMSerial.peek();
    LOG.print('.');
    Serial.print(',');
    
    if (gsm_rx_buf_line_cr) {
      if (c == LF) {
        GSMSerial.read();
      }
      gsm_rx_buf_full_line = true;
      break; // first prev line has to be processed
    }

    GSMSerial.read();
    if (c == CR) {
      gsm_rx_buf_full_line = true; // TODO this is workaround
      //gsm_rx_buf_line_cr = true;
      break; //continue;
    } else if (c == LF) {
      gsm_rx_buf_full_line = true;
      break;
    } else {
      // just general letter
      if (gsm_rx_buf_size >= GSM_RX_BUF_SIZE) {
        gsm_rx_buf_overflow = true;
      } else {
        gsm_rx_buf[gsm_rx_buf_size] = c;
        gsm_rx_buf_size++;
      }
    }
  }
}

void GSMUtils::resetGsmRxBufAfterLineRead()
{
  cli();
  gsm_rx_buf_size = 0;
  gsm_rx_buf_full_line = false;
  gsm_rx_buf_overflow = false;
  gsm_rx_buf_line_cr = false;
  sei();
}

bool GSMUtils::gsmIsLine(const char* templateStr, int templateSize)
{
  if (gsm_rx_buf_size != templateSize) {
    return false;
  }
  return gsmIsLineStartsWith(templateStr, templateSize);
}

bool GSMUtils::gsmIsLineStartsWith(const char* templateStr, int templateSize)
{
  if (gsm_rx_buf_size < templateSize) {
    return false;
  }
  for (byte i = 0; i < templateSize; i++) {
    if (gsm_rx_buf[i] != *(templateStr+i))
      return false;
  }
  return true;
}

int GSMUtils::gsmReadLineInteger(int templateSize)
{
  if (gsm_rx_buf_size > (templateSize+3)) {
    return -1;
  }
  int res = 0;
  for (byte i = 0; i < (gsm_rx_buf_size-templateSize); i++) {
    char c = gsm_rx_buf[gsm_rx_buf_size-1-i];
    LOG.print('['); LOG.print(c); LOG.print(']');
    if (c < '0' || c > '9') {
      return -1;
    }
    byte d = (byte)c - (byte)'0';
    LOG.print('['); LOG.print(d); LOG.print(']');
    int decPow = 1; for (int j = 0; j < i; j++) decPow *= 10;
    res += decPow * d;
    LOG.print('['); LOG.print(decPow); LOG.print(']');
  }
  return res;
}

bool GSMUtils::isRxBufLineReady()
{
  return gsm_rx_buf_full_line;
}

byte GSMUtils::getRxBufSize()
{
  return gsm_rx_buf_size;
}

volatile char* GSMUtils::getRxBufHeader()
{
  return &gsm_rx_buf[0];
}


