#include <SoftwareSerial.h>
 
//SIM800 TX is connected to Arduino D8
#define SIM800_TX_PIN 8
 
//SIM800 RX is connected to Arduino D7
#define SIM800_RX_PIN 7
 
//Create software serial object to communicate with SIM800
SoftwareSerial SIM(SIM800_TX_PIN,SIM800_RX_PIN);

char* text;
char* number;
String _buffer;

void setup() {
  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  while(!Serial);
   
  //Being serial communication witj Arduino and SIM800
  SIM.begin(9600);
  delay(1000);
   
  Serial.println("Setup Complete!");
  if (false) {
    _buffer.reserve(255);
    delay(3000);
    Serial.println("Will send SMS in 15sec!!!!");
    delay(15000);
    text="Testing Sms 1";  //text for the message. 
    number="+380975411368"; //change to a valid number.
    bool error = _sendSms(number, text);
    Serial.print("Result: "); Serial.println(error);
    text="Testing Sms 2";  //text for the message. 
    number="+380975411368"; //change to a valid number.
    error = _sendSms(number, text);
    Serial.print("Result2: "); Serial.println(error);
  }
}

int _timeout;
String _readSerial() {
  _timeout=0;
  while  (!SIM.available() && _timeout < 12000  ) 
  {
    delay(13);
    _timeout++;
  }
  if (SIM.available()) {
   return SIM.readString();
  }
}

bool _sendSms(char* number,char* text) {

  SIM.println(F("AT+CMGF=1")); // set sms to text mode  
  _buffer=_readSerial();
  SIM.print(F("AT+CMGS=\"")); // command to send sms
  SIM.print(number);
  SIM.println(F("\""));
  _buffer=_readSerial(); 
  SIM.println (text);
  delay(100);
  SIM.print((char)26);
  _buffer=_readSerial();
  //expect CMGS:xxx   , where xxx is a number,for the sending sms.
  if (((_buffer.indexOf("CMGS") ) != -1 ) ){
    return true;
  }
  else {
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

bool printUnblockedStart(const String& s)// return false if error occured!
{
  return printUnblockedStart(s.c_str(), s.length());
}
bool printUnblockedStart(char* text, size_t text_length)// return false if error occured!
{
  if (_printUnblocked_state != 0) {
    _printUnblocked_state = 0xE1; // error 1
    return false;
  }
  _printUnblocked_text = text;
  _printUnblocked_text_length = text_length;
  _printUnblocked_writeEndLn = false;
  _printUnblocked_state = 0x11;
  return true;
}
bool printUnblockedStart(const __FlashStringHelper *ifsh) // return false if error occured!
{
  if (_printUnblocked_state != 0) {
    _printUnblocked_state = 0xE1; // error 1
    return false;
  }
  _printUnblocked_p = reinterpret_cast<PGM_P>(ifsh);
  _printUnblocked_n = 0;
  _printUnblocked_writeEndLn = false;
  _printUnblocked_state = 0x21;
}

bool printUnblockedDone()
{
  switch (_printUnblocked_state)
  {
    case 0x11: // print text from "char* text" pointer
        if (Serial.availableForWrite() > 4)
        {
          while(Serial.availableForWrite()>0 && _printUnblocked_text_length--)
          {
            if (!Serial.write(*_printUnblocked_text++))
            {
              _printUnblocked_state == 0xE2; break; // error 2
            }
          }
          if (_printUnblocked_text_length == 0)
          {
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
            else { done = true; _printUnblocked_state = 0xE3; break; } // error 3
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
            return false;
          else
          {
            Serial.write(0x0D); Serial.write(0x0A);
            //Serial.print("\x0D\x0A");
            return true;
          }
        }
        else
          return true;
    break;
    default:
      _printUnblocked_state = 0xEF; // state handling error!
      return true;
  }
}

byte printUnblockedError()
{
  if ((_printUnblocked_state & 0xF0) == 0xE0)
  {
    return _printUnblocked_state & 0x0F;
  }
}
 
void loop() {
  //Read SIM800 output (if available) and print it in Arduino IDE Serial Monitor
  if(SIM.available()){
    Serial.write(SIM.read());
  }
  //Read Arduino IDE Serial Monitor inputs (if available) and send them to SIM800
  if(Serial.available()){    
    SIM.write(Serial.read());
  }
}
