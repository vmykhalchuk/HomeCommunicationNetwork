// Present a "Will be back soon web page", as stand-in webserver.
// 2011-01-30 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
 
#include <EtherCard.h>

#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
// ethernet interface ip address
static byte myip[] = { 192,168,1,200 };
// gateway ip address
static byte gwip[] = { 192,168,1,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x61,0x61,0x61,0x2D,0x30,0x31 };

const char page[] PROGMEM =
"HTTP/1.0 503 Service Unavailable\r\n"
"Content-Type: text/html\r\n"
""
;

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
const int BUF_SIZE = 320; // max 400 (buffer is 500, 90 for header)

byte buf1[BUF_SIZE];
int buf1Size = 0;
byte buf2[BUF_SIZE];
int buf2Size = 0;

void setup(){
  Serial.begin(57600);
  Serial.println("\n[ethernet_logger]");
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");
#endif

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
}

void loop(){
  if (Serial.available())
  {
    buf1[buf1Size++] = Serial.read();
    buf2[buf2Size++] = Serial.read();
  }
  // wait for an incoming TCP packet, but ignore its contents
  if (ether.packetLoop(ether.packetReceive())) {
    memcpy_P(ether.tcpOffset(), page, sizeof page);
    
    memcpy(ether.tcpOffset() + (sizeof page - 1), buf1, buf1Size);
    ether.httpServerReply((sizeof page - 1) + buf1Size);
  }
}
