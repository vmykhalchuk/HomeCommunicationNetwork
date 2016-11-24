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

  SIM.println(F("AT+CMGF=1")); //set sms to text mode  
  _buffer=_readSerial();
  SIM.print(F("AT+CMGS=\""));  // command to send sms
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
