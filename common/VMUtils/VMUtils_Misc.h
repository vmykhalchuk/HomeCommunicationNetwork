#ifndef VMUtils_Misc_h
#define VMUtils_Misc_h

#include "Arduino.h"

#ifndef SERIAL_OUTPUT
  #define SERIAL_OUTPUT Serial
#endif
//#define SERIAL_DEBUG NullSerial

// GENERAL OUTPUT
#define _println(x) SERIAL_OUTPUT.println(x); SERIAL_OUTPUT.flush();
#define _printlnF(x, f) SERIAL_OUTPUT.println(x, f); SERIAL_OUTPUT.flush();
#define _print(x) SERIAL_OUTPUT.print(x);
#define _printF(x, f) SERIAL_OUTPUT.print(x, f);

#define _print_flush() SERIAL_OUTPUT.flush();

#ifdef SERIAL_DEBUG
  #define _debugln(x) SERIAL_DEBUG.println(x); SERIAL_DEBUG.flush();
  #define _debuglnF(x, f) SERIAL_DEBUG.println(x, f); SERIAL_DEBUG.flush();
  #define _debug(x) SERIAL_DEBUG.print(x);
  #define _debugF(x, f) SERIAL_DEBUG.print(x, f);
  
  #define _debug_flush() SERIAL_DEBUG.flush();
#else
  #define _debugln(x)
  #define _debuglnF(x, f)
  #define _debug(x)
  #define _debugF(x, f)

  #define _debug_flush()
#endif

/*// GENERAL OUTPUT
void println(const char x[]) { SERIAL_OUTPUT.println(x); }
void print(const char x[]) { SERIAL_OUTPUT.print(x); }

void println(float x) { SERIAL_OUTPUT.println(x); }
void print(float x) { SERIAL_OUTPUT.print(x); }

void println(double x) { SERIAL_OUTPUT.println(x); }
void print(double x) { SERIAL_OUTPUT.print(x); }

void println(char x) { SERIAL_OUTPUT.println(x); }
void print(char x) { SERIAL_OUTPUT.print(x); }
void println(unsigned char x) { SERIAL_OUTPUT.println(x); }
void print(unsigned char x) { SERIAL_OUTPUT.print(x); }

void println(int x) { SERIAL_OUTPUT.println(x); }
void print(int x, int range) { SERIAL_OUTPUT.print(x, range); }
void println(unsigned int x) { SERIAL_OUTPUT.println(x); }
void print(unsigned int x, int range) { SERIAL_OUTPUT.print(x, range); }

void println(long x) { SERIAL_OUTPUT.println(x); }
void print(long x, int range) { SERIAL_OUTPUT.print(x, range); }
void println(unsigned long x) { SERIAL_OUTPUT.println(x); }
void print(unsigned long x, int range) { SERIAL_OUTPUT.print(x, range); }
*/

/*// DEBUG OUTPUT
void debugln(const char x[]) { SERIAL_DEBUG.println(x); }
void debug(const char x[]) { SERIAL_DEBUG.print(x); }

void debugln(float x) { SERIAL_DEBUG.println(x); }
void debug(float x) { SERIAL_DEBUG.print(x); }

void debugln(double x) { SERIAL_DEBUG.println(x); }
void debug(double x) { SERIAL_DEBUG.print(x); }

void debugln(char x) { SERIAL_DEBUG.println(x); }
void debug(char x) { SERIAL_DEBUG.print(x); }
void debugln(unsigned char x) { SERIAL_DEBUG.println(x); }
void debug(unsigned char x) { SERIAL_DEBUG.print(x); }

void debugln(int x) { SERIAL_DEBUG.println(x); }
void debug(int x, int range) { SERIAL_DEBUG.print(x, range); }
void debugln(unsigned int x) { SERIAL_DEBUG.println(x); }
void debug(unsigned int x, int range) { SERIAL_DEBUG.print(x, range); }

void debugln(long x) { SERIAL_DEBUG.println(x); }
void debug(long x, int range) { SERIAL_DEBUG.print(x, range); }
void debugln(unsigned long x) { SERIAL_DEBUG.println(x); }
void debug(unsigned long x, int range) { SERIAL_DEBUG.print(x, range); }
*/

#endif