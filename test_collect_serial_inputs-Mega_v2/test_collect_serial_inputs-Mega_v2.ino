#include <SoftwareSerial.h>


#include <Arduino.h>
#include "Common.h"
#include "Functions.h"

// RX0 - this is pin for Serial input #0
// RX1 - this is pin for Serial input #1
// RX2 - this is pin for Serial input #2
// RX3 - this is pin for Serial input #3
// D52 (FIXME - this one is used for MISO!) - this is pin for Serial input #4

// D7 - this is pin for Serial output

// Not all pins on the Mega and Mega 2560 support change interrupts, so only the following can be used for RX: 10, 11, 12, 13, 14, 15, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69).
const int SSERIAL4_RX_PIN = 52; // FIXME - this one is used for MISO!
const int SSERIALOUT_TX_PIN = 7;

SoftwareSerial SSerial4(SSERIAL4_RX_PIN, 7);
SoftwareSerial SSerialOut(5, SSERIALOUT_TX_PIN);

SerialState states[SERIALS_COUNT];
Stream* serials[SERIALS_COUNT] = {&Serial, &Serial1, &Serial2, &Serial3, &SSerial4};

void setup()
{
  delay(1000);
  
  for (byte i = 0; i < SERIALS_COUNT; i++)
  {
    states[i].ticksPassedSinceLastByteReceived = 0;
    states[i].lastByteReceived = 0;
    states[i].buf[0] = 0;
    states[i].startIndx = 0;
    states[i].writeToIndx = 0;
    states[i].writeToEnd = 0;
    states[i].bufOverflowCounter = 0;
  }

  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(SERIAL_RX_BODS);
  while(!Serial);
  Serial1.begin(SERIAL_RX_BODS);
  while(!Serial1);
  Serial2.begin(SERIAL_RX_BODS);
  while(!Serial2);
  Serial3.begin(SERIAL_RX_BODS);
  while(!Serial3);

  SSerialOut.begin(SERIAL_TX_BODS); // this one will be used only for writing
  SSerial4.begin(SERIAL_RX_BODS); // this one will be actively listening

   
  SSerialOut.println("L: Setup Complete!");
}

bool isWritingLine(byte serialNo)
{
  return states[serialNo].writeToIndx != states[serialNo].writeToEnd;
}


//            case 1                                          case 2                                 case 1b (addon to case 1)
//   Still writing                                We need at least 3B for new row                     Writing is complete
//                                                So new row cannot be started
//  0  1  2  3  4  5  6  7  8  9                 0  1  2  3  4  5  6  7  8  9  10                 0  1  2  3  4  5  6  7  8  9
//  O  O  O  O  O  O  O  O  O  O  <- Buffer  ->  O  O  O  O  O  O  O  O  O  O  O   <- Buffer  ->  O  O  O  O  O  O  O  O  O  O
//  D  WI D  D  WE SI D  D  D  D                 SI D  D  D  D  D  D  D  D  WZ x                  D  D  D  D  D  D  SI D  D  D
//                                                                                                                  WZ
//  3  4  4  4  4  3  3  3  3  3  <- Record# ->  5  5  5  5  5  6  6  6  6  ?      <- Record# ->  3  4  4  4  5  5  3  3  3  3
//
//  WI - WriteToIndex;  WE - WriteToEnd;  SI - StartIndex
//  WZ - WriteToIndex / WriteToEnd;       
//  D - Data;  x - Empty cell
bool isBufferFull(byte serialNo, byte bytesNeeded)
{
  unsigned int start = states[serialNo].startIndx;
  unsigned int we = states[serialNo].writeToEnd;

//  if (isWritingLine(serialNo))
//  {
//    we = (we + 1) % BUF_SIZE;
//  }
//  else
//  {
//    //if (start == we) return false; // all pointers are in one place - meaning we have totally empty buffer - NOTE or it is wrong and buffer is totally full!
//  }
  we = isWritingLine(serialNo) ? ((we + 1) % BUF_SIZE) : we;
  if (start == we) {
    return true; //unless we have fully empty buffer
  }
  
  return start > we ? (start - we < bytesNeeded) : (start + BUF_SIZE - we < bytesNeeded);
}

// this function assumes that there is space in buffer!
// and that we are writing line! (isWritingLine() returns true!
void writeByte(byte serialNo, byte b)
{
  unsigned int wti = states[serialNo].writeToIndx;
  unsigned int wte = (states[serialNo].writeToEnd + 1) % BUF_SIZE;
  states[serialNo].buf[wte] = b;
  states[serialNo].buf[wti]++;
  states[serialNo].writeToEnd = wte;

  if (states[serialNo].buf[wti] == 0xFF) endTheLine(serialNo, END_LINE_REASON::tooLong);
}

// this function assumes that there is space in buffer to start new line!
// and that we are not writing line!
// b[0] - size of line
// b[1] - system byte:
//          b0 and b1 are two higher bits of _time
//          b2, b3, b4 are 3 bits for next buffer to read from (0b111 - none - meaning no more to read yet)
//          b5 - bit for time overflow (when set - we have to decrement one minute from current time's minute/hour/day)
//          b6 and b7 - 2 bits for the type of line ending (see END_LINE_REASON)
// b[2] - 8 lower bits of _time
void startTheLine(byte serialNo, unsigned int _time)
{
  unsigned int wti = states[serialNo].writeToIndx;
  unsigned int wte = (states[serialNo].writeToEnd + 3) % BUF_SIZE;
  states[serialNo].buf[wti] = 0;
  states[serialNo].buf[wti+1] = (_time >> 8) & 0b00000011; // store time higher bits
  states[serialNo].buf[wti+1] |= 0b111 << 2;// store next buffer to read from (0b111 - meaning none)
  states[serialNo].buf[wti+2] = _time % 0xFF;
  states[serialNo].writeToEnd = wte;

  // now we have to update current writeTo buffer (its last written line) with this buffer No
  if (lastStartedWriteToThread == 0xFF) {
    // FIXME what if prev thread has already finished writing and switched writeToIndx to empty slot!
    states[lastStartedWriteToThread].buf[states[lastStartedWriteToThread].writeToIndx+1] |= (serialNo) & 0b111  << 2;
  }
  lastStartedWriteToThread = serialNo;
}

void endTheLine(byte serialNo, byte reason)
{
  states[serialNo].writeToEnd = (states[serialNo].writeToEnd + 1) % BUF_SIZE;
  states[serialNo].writeToIndx = states[serialNo].writeToEnd;
}

void markAllBufferLinesAsTimeOverflow(byte serialNo)
{
  // we should mark every system byte with timerOverflow flag - meaning that one minute has passed, that has to be counted when reading this row
}

void incrementBufOverflow(byte serialNo)
{
  if (states[serialNo].bufOverflowCounter < 0xFFFE) states[serialNo].bufOverflowCounter++; else states[serialNo].bufOverflowCounter = 0xFFFF;
}

void loop()
{
  unsigned int _time = 0;//define time here - it should be deciseconds (0..600 for every minute)
  for (byte serialNo = 0; serialNo < SERIALS_COUNT; serialNo++)
  {
    bool iwl = isWritingLine(serialNo);
    if (iwl)
    {
      if (++(states[serialNo].ticksPassedSinceLastByteReceived) >= SERIAL_TICKS_TIMEOUT)
      {
        // eol (even no line end detected)
        endTheLine(serialNo, END_LINE_REASON::noEOLReceived);
        states[serialNo].lastByteReceived = 0; // when we timeout - then we have received no byte - default to 0
      }
    }
    
    if (serials[serialNo]->available())
    {
      int c = serials[serialNo]->read();
      if (c < 0 || c > 255) {
        // FIXME Log it somehow: // this is exceptional situation reading from stream!
        c = 0;
      }
      byte b = c;
      if (b == 0x0A)
      {
        if (states[serialNo].lastByteReceived == 0x0D) {
          // just ignore this one - we already closed the line
        } else {
          bool canWrite = true;
          if (!iwl) { canWrite = !isBufferFull(serialNo, 3); if (canWrite) startTheLine(serialNo, _time); else incrementBufOverflow(serialNo); }
          if (canWrite) endTheLine(serialNo, END_LINE_REASON::normal);
        }
      }
      else if (b == 0x0D)
      {
          bool canWrite = true;
          if (!iwl) { canWrite = !isBufferFull(serialNo, 3); if (canWrite) startTheLine(serialNo, _time); else incrementBufOverflow(serialNo); }
          if (canWrite) endTheLine(serialNo, END_LINE_REASON::normal);
      }
      else
      {
        bool canWrite = true;
        if (!iwl) { canWrite = !isBufferFull(serialNo, 3); if (canWrite) startTheLine(serialNo, _time); }
        if (canWrite) { canWrite = !isBufferFull(serialNo, 1); if (canWrite) writeByte(serialNo, b); else endTheLine(serialNo, END_LINE_REASON::bufferOverflow); }
        if (!canWrite) incrementBufOverflow(serialNo);
      }
      
      states[serialNo].lastByteReceived = b;
    }
  }

  // now write to output stream if anything available
}

