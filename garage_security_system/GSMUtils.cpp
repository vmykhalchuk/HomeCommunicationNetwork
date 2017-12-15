#include "Arduino.h"
#include "GSMUtils.h"

#define GSMSerial (*gsmComm)

GSMUtils::GSMUtils(Stream* gsmComm)
{
  this->gsmComm = gsmComm;
}

void GSMUtils::_serialEvent()
{
  if (gsm_rx_buf_full_line) return; // we have to wait before line is processed

  if (gsm_rx_buf_line_cr && (z++ > 500)) // TODO set it to 1000, or tune it accordingly to speed of COM port
  {
    _debug('!'); _debug_flush();
    if (gsm_rx_buf_line_cr) {
      gsm_rx_buf_full_line = true;
      return;
    }
  }
  
  while (GSMSerial.available())
  {
    z = 0;
    char c = (char)GSMSerial.peek();
    _debug('.'); _debug_flush();
    
    if (gsm_rx_buf_line_cr) {
      if (c == LF) {
        GSMSerial.read();
      }
      gsm_rx_buf_full_line = true;
      break; // first prev line has to be processed
    }

    GSMSerial.read();
    if (c == CR) {
      //gsm_rx_buf_full_line = true; // TODO this is workaround
      gsm_rx_buf_line_cr = true;
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

void GSMUtils::resetGsmRxChannel()
{
  cli();
  while (GSMSerial.available()) GSMSerial.read();
  resetGsmRxBufAfterLineRead();
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

bool GSMUtils::gsmIsSMSTypeTextRequest()
{
  if (gsm_rx_buf_size != 2) return false;
  return (gsm_rx_buf[0] == '>');
}

bool GSMUtils::gsmIsEmptyLine()
{
  return gsm_rx_buf_size == 0;
}

bool GSMUtils::gsmIsLine(const char* templateStr, int templateSize)
{
  if (gsm_rx_buf_size != templateSize) {
    return false;
  }
  return gsmIsLineStartsWith(templateStr, templateSize);
}

byte GSMUtils::gsmIsOneOfLines(GSMUtilsStringPointer str1)
{
	if (this->gsmIsLine(str1.templateStr, str1.templateSize))
	{
		return 1;
	}
	else
	{
		return 0;
	}
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
    //_debug('['); _debug(c); _debug('-');
    if (c < '0' || c > '9') {
      return -1;
    }
    byte d = (byte)c - (byte)'0';
    //_debug(d); _debug('-');
    int decPow = 1; for (int j = 0; j < i; j++) decPow *= 10;
    res += decPow * d;
    //_debug(decPow); _debugln(']');
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

char* GSMUtils::getRxBufHeader()
{
  return &gsm_rx_buf[0];
}


