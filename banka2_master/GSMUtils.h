#ifndef GSMUtils_h
#define GSMUtils_h

#include "Arduino.h"
#include "Common.h"
#include <Stream.h>

#define CR '\x0D' // CR; CR+LF; LF
#define LF '\x0A'
#define GSM_RX_BUF_SIZE 250
#define GSM_TX_BUF_SIZE 250

const char GSM_OK[] = {'O', 'K'};
const char GSM_CPIN_READY[] = {'+','C','P','I','N',':',' ','R','E','A','D','Y'};
const char GSM_CALL_READY[] = {'C','a','l','l',' ','R','e','a','d','y'};
const char GSM_SMS_READY[] = {'S','M','S',' ','R','e','a','d','y'};

const char GSM_CMGS[] = {'+','C','M','G','S',':',' '};

class GSMUtils
{
  public:
    GSMUtils(Stream* gsmComm, Stream* logComm);
    void _serialEvent();
    void resetGsmRxChannel();
    void resetGsmRxBufAfterLineRead();

    bool isRxBufLineReady();
    byte getRxBufSize();
    volatile char* getRxBufHeader();
    Stream* getGsmComm() {return gsmComm;}
  private:
    Stream* gsmComm;
    Stream* logComm;
    volatile char gsm_rx_buf[GSM_RX_BUF_SIZE];
    volatile byte gsm_rx_buf_size;
    volatile bool gsm_rx_buf_full_line = false;
    volatile bool gsm_rx_buf_overflow = false;
    volatile bool gsm_rx_buf_line_cr = false;

    volatile int z = 0;

  public:
    bool gsmIsEmptyLine();
    bool gsmIsLine(const char* templateStr, int templateSize);
    bool gsmIsLineStartsWith(const char* templateStr, int templateSize);
    int gsmReadLineInteger(int templateSize);
};

#endif
