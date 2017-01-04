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
const char GSM_ERROR[] = {'E', 'R', 'R', 'O', 'R'};
const char GSM_CPIN_READY[] = {'+','C','P','I','N',':',' ','R','E','A','D','Y'};
const char GSM_CALL_READY[] = {'C','a','l','l',' ','R','e','a','d','y'};
const char GSM_SMS_READY[] = {'S','M','S',' ','R','e','a','d','y'};
const char GSM_CTZU[] = {'+', 'C', 'T', 'Z', 'U', ':', ' ', '"'}; // +CTZU: "15/05/06,17:25:42",-12,0

class GSMUtils
{
  public:
    GSMUtils(Stream* gsmComm, Stream* logComm);
    void _serialEvent();
    void resetGsmRxChannel();
    void resetGsmRxBufAfterLineRead();

    bool isRxBufLineReady();
    byte getRxBufSize();
    char* getRxBufHeader();
    Stream* getGsmComm() {return gsmComm;}
  private:
    Stream* gsmComm;
    Stream* logComm;
    char gsm_rx_buf[GSM_RX_BUF_SIZE];
    byte gsm_rx_buf_size;
    bool gsm_rx_buf_full_line = false;
    bool gsm_rx_buf_overflow = false;
    bool gsm_rx_buf_line_cr = false;

    int z = 0;

  public:
    char gsm_sendCharBuf[250];
    byte gsm_sendCharBuf_size = 0;
    bool gsmIsSMSTypeTextRequest();
    bool gsmIsEmptyLine();
    bool gsmIsLine(const char* templateStr, int templateSize);
    bool gsmIsLineStartsWith(const char* templateStr, int templateSize);
    int gsmReadLineInteger(int templateSize);
};

#endif
