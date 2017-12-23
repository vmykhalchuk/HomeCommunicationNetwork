#ifndef VMUtils_ADC_h
#define VMUtils_ADC_h

#include "Arduino.h"

class VMUtils_ADC
{
  private:
    static void _readVcc();
  public:
    /**
	 * Reads VCC voltage and compares to REF voltage (assumes it is 1.1V) and calculates current VCC.
	 * NOTE: Due REF voltage not being the same for all ATMega chips - the reading are not accurate, however stable for same device.
	 */
    static float readVcc();
	/**
	 * This method is advanced version of readVcc():float because returns direct readings from ADC.
	 * You then have to calculate VCC by yourself with this code:
	 *   float vcc = 1.1 / float (vccAdc + 0.5) * 1024.0;
	 * NOTE: 1.1 - is a REF voltage in volts of a given device. You might need to adjust this value for you specific device
	 */
	static uint16_t readVccAsUint();
    //static void readVcc(uint8_t& lowByte, uint8_t& highByte);

};

#endif
