#include <SoftwareSerial.h>
 
//SIM800 TX is connected to Arduino D8
#define SIM800_TX_PIN 8
 
//SIM800 RX is connected to Arduino D7
#define SIM800_RX_PIN 7
 
#define LOG_RX 7
#define LOG_TX 8
SoftwareSerial LOG(LOG_RX, LOG_TX);

char* text;
char* number;
String _buffer;

void setup() {
  Serial.begin(9600);
  while(!Serial);
   
  LOG.begin(9600);
  delay(1000);
   
  LOG.println("Setup Complete!");
  if (true) {
    _buffer.reserve(255);
    delay(3000);
    LOG.println("Will send SMS in 15sec!!!!");
    delay(15000);
    text="Testing Sms 1";  //text for the message. 
    number="+380975411368"; //change to a valid number.
    bool error = _sendSms(number, text);
    LOG.print("Result: "); LOG.println(error);
  }
}

int _timeout;
String _readSerial(void) {
  _timeout=0;
  while  (!Serial.available() && _timeout < 12000  ) 
  {
    delay(13);
    _timeout++;
  }
  if (Serial.available()) {
   return Serial.readString();
  }
}

void checkError(void)
{
  byte err = printUnblockedError();
  if (err)
  {
    LOG.print("Error: ");
    LOG.println(err, HEX);
    LOG.flush();
    delay(1000);
  }
}

void printUnblockedWait(void)
{
  do {checkError();} while(!printUnblockedDone());
}

bool _sendSms2(char* number, char* text) {

  printUnblockedStartLn("AT+CMGF=1"); printUnblockedWait(); // set sms to text mode
  _buffer=_readSerial();
  printUnblockedStart("AT+CMGS=\""); printUnblockedWait(); // command to send sms
  printUnblockedStart(number); printUnblockedWait(); 
  printUnblockedStartLn("\""); printUnblockedWait();
  _buffer=_readSerial(); 
  printUnblockedStartLn(text); printUnblockedWait();
  delay(100);
  printUnblockedStart("\x1A"); printUnblockedWait();//(char)26
  _buffer=_readSerial();
  //expect CMGS:xxx   , where xxx is a number,for the sending sms.
  if (((_buffer.indexOf("CMGS") ) != -1 ) ){
    return true;
  }
  else {
    return false;
  }
}

bool _sendSms(char* number, char* text)
{
  while(!_write('A')); while(!_write('T')); while(!_write('+')); while(!_write('C')); while(!_write('\x1A'));
  Serial.println();
  char z = '\x1A';
  LOG.print((byte)z, DEC); LOG.print('-'); LOG.println((byte)z, HEX);
  LOG.flush();
}

char* _writeStr__str = 0;
byte _writeStr__l = 0;
byte _writeStr__i = 0;
void _writeStr_start(char* str, byte l)
{
  _writeStr__str = str;
  _writeStr__l = l;
  _writeStr__i = 0;
}

boolean _writeStr_done()
{
  
}

boolean _write(char c)
{
  if (Serial.availableForWrite() > 0)
  {
    Serial.write(c);
    return true;
  }
  else
  {
    return false;
  }
}

// for char*,sie_t
volatile char* _printUnblocked_text;
volatile size_t _printUnblocked_text_length;
// for __FlashStringHelper *
volatile PGM_P _printUnblocked_p;
volatile size_t _printUnblocked_n;
// common
volatile bool _printUnblocked_writeEndLn;
volatile byte _printUnblocked_state = 0;
volatile byte _printUnblocked_error = 0;

bool printUnblockedStart(const String& s)// return false if error occured!
{
  return _printUnblockedStart(s.c_str(), s.length(), false);
}
bool printUnblockedStartLn(const String& s)// return false if error occured!
{
  return _printUnblockedStart(s.c_str(), s.length(), true);
}
bool _printUnblockedStart(const char* text, size_t text_length, bool endLn)// return false if error occured!
{
  LOG.print("M1."); LOG.flush();
  if (_printUnblocked_state != 0) {
    LOG.print("E1."); LOG.flush();
    _printUnblocked_state = 0xE0; _printUnblocked_error = 1;
    return false;
  }
  _printUnblocked_text = text;
  _printUnblocked_text_length = text_length;
  _printUnblocked_writeEndLn = endLn;
  _printUnblocked_state = 0x11;
  return true;
}
//bool printUnblockedStartLn(const __FlashStringHelper *ifsh) // return false if error occured!
//{
//  _printUnblockedStart(ifsh, true);
//}
//bool printUnblockedStart(const __FlashStringHelper *ifsh) // return false if error occured!
//{
//  _printUnblockedStart(ifsh, false);
//}
bool _printUnblockedStart(const __FlashStringHelper *ifsh, boolean endLn) // return false if error occured!
{
  if (_printUnblocked_state != 0) {
    _printUnblocked_state = 0xE0; _printUnblocked_error = 1;
    return false;
  }
  _printUnblocked_p = reinterpret_cast<PGM_P>(ifsh);
  _printUnblocked_n = 0;
  _printUnblocked_writeEndLn = false;
  _printUnblocked_state = 0x21;
}

bool printUnblockedDone()
{
  LOG.print('#'); LOG.print(_printUnblocked_state, HEX); LOG.print('#'); LOG.flush();
  switch (_printUnblocked_state)
  {
    case 0x11: // print text from "char* text" pointer
        if (Serial.availableForWrite() > 4)
        {
          while((Serial.availableForWrite()>0) && (_printUnblocked_text_length--))
          {
            LOG.print(_printUnblocked_text_length, DEC); LOG.print(':');
            LOG.write((char) (*_printUnblocked_text)); LOG.print('.'); LOG.flush();
            if (!Serial.write((char) *_printUnblocked_text++))
            {
              LOG.print('!');
              _printUnblocked_state = 0xE0; _printUnblocked_error = 2; break; // error 2
            }
          }
          if (_printUnblocked_text_length == 0)
          {
            LOG.print('!');LOG.print('#');
            _printUnblocked_state = 0x01; // write EndLine if required
          }
        }
        return false;
    break;
    case 0x21: // print text from "__FlashStringHelper*"
        if (Serial.availableForWrite() > 4)
        {
          int z = 0;
          bool done = false;
          while(Serial.availableForWrite()>0 && z++ < 150) {
            unsigned char c = pgm_read_byte(_printUnblocked_p++);
            if (c == 0) { done = true; break; }
            if (Serial.write(c)) _printUnblocked_n++;
            else { done = true; _printUnblocked_state = 0xE0; _printUnblocked_error = 3; break; } // error 3
          }
          if (done) {
            _printUnblocked_state = 0x01; // write EndLine if required
          }
        }
        return false;
    break;
    case 0x01: // print endline if required
        if (_printUnblocked_writeEndLn)
        {
          if (Serial.availableForWrite() < 2)
          {
            return false;
          }
          else
          {
            Serial.write(0x0D); Serial.write(0x0A);
            //Serial.print("\x0D\x0A");
            _printUnblocked_state = 0;
            return true;
          }
        }
        else
        {
          _printUnblocked_state = 0;
          return true;
        }
    break;
    default:
      // state handling error!
      _printUnblocked_state = 0xE0; _printUnblocked_error = 0xF;
      return true;
  }
  return false;
}

byte printUnblockedError()
{
  if (_printUnblocked_state == 0xE0)
  {
    return _printUnblocked_error;
  }
  return 0;
}
 
void loop() {
  if(LOG.available()){
    Serial.write(LOG.read());
  }
  if(Serial.available()){    
    LOG.write(Serial.read());
  }
}
