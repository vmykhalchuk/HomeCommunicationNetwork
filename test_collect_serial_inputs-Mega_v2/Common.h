const unsigned int SERIAL_RX_BODS = 57600;
const unsigned int SERIAL_TX_BODS = 57600; // we can make it twice faster for more reliable operation

const unsigned int SERIAL_TICKS_TIMEOUT = 2048; // when that many ticks passed since last received character - this means that serial line is over
const unsigned int BUF_SIZE = 1024;

enum END_LINE_REASON {
  normal = 0, // when EOL received (either CRLF, CR or LF)
  tooLong, // when more then 255 bytes received
  noEOLReceived, // when no EOL received
  bufferOverflow // when buffer has overflown and no more space to write to
};

struct SerialState
{
  unsigned int ticksPassedSinceLastByteReceived = 0;

  // EOL might be one of:
  // 0D+0A (CR+LF)
  // 0A    (LF)
  // 0D    (CR)
  byte lastByteReceived = 0;

  byte buf[BUF_SIZE];
  
  unsigned int startIndx = 0;   // this is where buffer starts; writeToEnd - is where it ends, unless writeToEnd==writeToIndx - in this case buffer ends at writeToEnd-1 (take into consideration wrapping)
  unsigned int writeToIndx = 0; // when writeToIndx == writeToEnd - then we are waiting on position to start next line (unless buffer is full and we are waiting on startIndx position)
  unsigned int writeToEnd = 0;

  unsigned int bufOverflowCounter = 0;
};

const byte SERIALS_COUNT = 5;

byte lastStartedWriteToThread = 0xFF; // where to search for record (writeToIndx) to be updated with next-to-read-thread value

byte currentReadFromThread = 0; // 0 nothing to read yet

