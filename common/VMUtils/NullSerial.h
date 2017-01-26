#ifndef NullSerial_h
#define NullSerial_h

#include "Arduino.h"

class NullSerial : public Stream
{
  public:
    virtual inline int peek() {return 0;}
    virtual inline size_t write(uint8_t byte) {(void)byte;return 0;}
    virtual inline int read() {return 0;}
    virtual int available() {return 0;}
    virtual void flush() {};
};

#endif