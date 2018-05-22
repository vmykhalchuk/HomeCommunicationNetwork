//http://cpp.sh/
//https://ideone.com
//http://codepad.org
//https://www.onlinegdb.com/#
#include <stdio.h>
#include <stdint.h>
#include <algorithm>

using namespace std;
#define SERIAL_DEBUG 1
#define _debug(x) printf(x);
#define _debugln(x) printf("\n");
#define _debug__dig(x) printf("%u", x);
#define _debugF(x,y) printf("%x", x);













int main()
{
    uint8_t t = 255;
    printf("Hello World! %u\n", t);

    initDataBuf1and2();
    for (int i = 30; i <= 1230; i+= 30) {
        printf("i=%u\n", i);
        updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_1[0], (i < 1000 ? 5 : 10), i);
        updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_2[0], (i== 30 ? 1 : 0), i);
    }

    debugStoredData();
    
    
    printf("Sending transmission:");
    uint8_t transmission[32];
    for (int i = 0; i < 32; i++) {
        transmission[i] = (uint8_t)151;
    }
    

  uint8_t * transmitBufPointer = &transmission[4];
  int pos = 0;
  for (int i = 0; i < 15; i++) {
    storeOnPosition(transmitBufPointer, pos++, i+10);
  }

  for (int i = 0; i < 15; i++) debugPrintPos(&transmission[4], i); 


    /*loadToTransmitBuf(&data_buf_1[0], &transmission[4]);   // 8 bytes for light
    loadToTransmitBuf(&data_buf_2[0], &transmission[4+8]); // 8 bytes for magnetometer
    loadToTransmitBuf(&data_buf_3[0], &transmission[4+16]);// 8 bytes for reset
    
    processRadioTransmission_debug(&transmission[0]);
    */
    return 0;
}
