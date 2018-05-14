//https://www.onlinegdb.com/#
#include <stdio.h>
#include <stdint.h>
#include <algorithm>

const uint8_t data_buf_size = 20+16+32+128;
uint8_t data_buf_1[data_buf_size]; // used by light
uint8_t data_buf_2[data_buf_size]; // used by magnetometer
uint8_t data_buf_3[data_buf_size]; // used by reset
void initDataBuf1and2()
{
  for (int i = 0; i < data_buf_size; i++)
  {
    data_buf_1[i] = 0;
    //data_buf_2[i] = 0;
    data_buf_3[i] = 0;
  }
}

void shift2mIntervals(uint8_t * data_buf_pointer) {
  for (int i = 19; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift10mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20;
  for (int i = 15; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift20mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20 + 16;
  for (int i = 31; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift40mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20 + 16 + 32;
  for (int i = 127; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift2mIntervalAndRecalculate(uint8_t * data_buf_pointer, uint8_t newData, long secondsSinceStart)
{
  // shift 2m intervals to the right (first 2m interval will be set to newData)
  shift2mIntervals(data_buf_pointer);
  *(data_buf_pointer) = newData;

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m

  // update 10m intervals
  bool shift10m = secondsSinceStart % 600 == 0;
  if (shift10m) shift10mIntervals(data_buf_pointer);
  uint8_t offset2m_for_10m = 0;
  for (int i = 0; i < 4; i++) // recalculate first 4 10m intervals from 2m intervals
  {
    uint8_t val10m_i = *(data_buf_pointer + offset2m_for_10m);
    for (int j = 1; j < 5; j++)
    {
      val10m_i = std::max(val10m_i, *(data_buf_pointer + offset2m_for_10m + j));
    }
    *(data_buf_pointer + 20 + i) = val10m_i;
    offset2m_for_10m += 5;
  }

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m

  // update 20m intervals
  bool shift20m = secondsSinceStart % 1200 == 0;
  if (shift20m) shift20mIntervals(data_buf_pointer);
  uint8_t offset_10m_for_20m = 20;
  for (int i = 0; i < (shift10m ? 8 : 2); i++) // recalculate first 2 20m intervals from 4 first 10m intervals (8 20m intervals has to be recalculated if 10m intervals were shifted)
  {
    uint8_t val20m_i = *(data_buf_pointer + offset_10m_for_20m);
    for (int j = 1; j < 2; j++)
    {
      val20m_i = std::max(val20m_i, *(data_buf_pointer + offset_10m_for_20m + j));
    }
    *(data_buf_pointer + 20 + 16 + i) = val20m_i;
    offset_10m_for_20m += 2;
  }

  // update 40m intervals
  bool shift40m = secondsSinceStart % 2400 == 0;
  if (shift40m) shift40mIntervals(data_buf_pointer);
  uint8_t offset_20m_for_40m = 20 + 16;
  for (int i = 0; i < (shift20m ? 16 : 1); i++) // recalculate first 40m interval from 2 first 20m intervals (16 40m intervals has to be recalculated if 20m intervals were shifted)
  {
    uint8_t val40m_i = *(data_buf_pointer + offset_20m_for_40m);
    for (int j = 1; j < 2; j++)
    {
      val40m_i = std::max(val40m_i, *(data_buf_pointer + offset_20m_for_40m + j));
    }
    *(data_buf_pointer + 20 + 16 + 32 + i) = val40m_i;
    offset_20m_for_40m += 2;
  }
}

void updateRecent2mIntervalAndShiftWhenNeeded(uint8_t * data_buf_pointer, uint8_t newData, long secondsSinceStart)
{
  bool shift2m = secondsSinceStart % 120 == 0;
  if (shift2m)
  {
    // 2m interval, now we need to shift
    shift2mIntervalAndRecalculate(data_buf_pointer, newData, secondsSinceStart);
  }
  else
  {
    // update first 2m interval
    *data_buf_pointer = std::max(newData, *data_buf_pointer);
  
    // update first 10m interval
    uint8_t int10m_0 = *data_buf_pointer;
    for (int i = 1; i < 5; i++) {
      uint8_t int2m_i = *(data_buf_pointer + i);
      int10m_0 = std::max(int10m_0, int2m_i);
    }
    *(data_buf_pointer + 20) = int10m_0;
  
    // update first 20m interval
    uint8_t int20m_0 = std::max(*(data_buf_pointer + 20 + 0), *(data_buf_pointer + 20 + 1));
    *(data_buf_pointer + 20 + 16) = int20m_0;
  
    // update first 40m interval
    uint8_t int40m_0 = std::max(*(data_buf_pointer + 20 + 16 + 0), *(data_buf_pointer + 20 + 16 + 1));
    *(data_buf_pointer + 20 + 16 + 32) = int40m_0;
  }
}




void __debugPrintPoint(uint8_t z) {
  if (z > 10) printf("%u", z / 10); else printf("0");
  printf("%u", z % 10);
  printf("-");
}
void __debugStoredData(uint8_t * data_buf_pointer) {
                            printf("2m[20]: "); for (int i = 0; i < 20; i++) __debugPrintPoint(*(data_buf_pointer+i)); printf("\n");
  printf("              "); printf("10m[16]: "); for (int i = 0; i < 16; i++) __debugPrintPoint(*(data_buf_pointer+20+i)); printf("\n");
  printf("              "); printf("20m[32]: "); for (int i = 0; i < 16; i++) __debugPrintPoint(*(data_buf_pointer+20+16+i)); printf("\n");
  printf("              "); printf("40m[128]: "); for (int i = 0; i < 16; i++) __debugPrintPoint(*(data_buf_pointer+20+16+32+i)); printf("\n");
}
void debugStoredData() {
    printf("Buf_1(Light): "); __debugStoredData(&data_buf_1[0]);
    printf("Buf_2(Magnt): "); __debugStoredData(&data_buf_2[0]);
}


int main()
{
    uint8_t t = 255;
    printf("Hello World! %u\n", t);

    initDataBuf1and2();
    for (int i = 30; i <= 1230; i+= 30) {
        printf("i=%u\n", i);
        updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_1[0], 5, i);
        updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_2[0], (i== 30 ? 1 : 0), i);
    }

    debugStoredData();
    return 0;
}

