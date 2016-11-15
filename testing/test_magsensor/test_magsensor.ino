/***************************************************************************
  This is a library example for the HMC5883 magnentometer/compass

  Designed specifically to work with the Adafruit HMC5883 Breakout
  http://www.adafruit.com/products/1746
 
  *** You will also need to install the Adafruit_Sensor library! ***

  These displays use I2C to communicate, 2 pins are required to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Kevin Townsend for Adafruit Industries with some heading example from
  Love Electronics (loveelectronics.co.uk)
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the version 3 GNU General Public License as
 published by the Free Software Foundation.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 ***************************************************************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

/* Assign a unique ID to this sensor at the same time */
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

void displaySensorDetails(void)
{
  sensor_t sensor;
  mag.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" uT");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" uT");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" uT");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

void setup(void) 
{
  Serial.begin(9600);
  Serial.println("HMC5883 Magnetometer Test"); Serial.println("");
  
  /* Initialise the sensor */
  if(!mag.begin())
  {
    /* There was a problem detecting the HMC5883 ... check your connections */
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while(1);
  }
  
  /* Display some basic information on this sensor */
  displaySensorDetails();
  pinMode(13, OUTPUT);
}

float data[15][3];
int head = 0;
boolean initialized = false;

void loop(void) 
{
  /* Get a new sensor event */ 
  sensors_event_t event; 
  mag.getEvent(&event);
 
  /* Display the results (magnetic vector values are in micro-Tesla (uT)) */
  Serial.print("X: "); Serial.print(event.magnetic.x); Serial.print("  ");
  Serial.print("Y: "); Serial.print(event.magnetic.y); Serial.print("  ");
  Serial.print("Z: "); Serial.print(event.magnetic.z); Serial.print("  ");
  Serial.println("uT");

  data[head][0] = event.magnetic.x;
  data[head][1] = event.magnetic.y;
  data[head][2] = event.magnetic.z;
  
  head++;
  
  if (!initialized) {
    if (head == 15) {
      initialized = true;
    }
  }
  head = head % 15;

  bool alarm = false;

  if (initialized) {
    // now we can start algorithm
    float sumXTotal = 0;
    float sumYTotal = 0;
    float sumZTotal = 0;
    float sumXLastThree = 0;
    float sumYLastThree = 0;
    float sumZLastThree = 0;
    int pos = head;
    for (int i = 0; i < 15; i++) {
      sumXTotal += data[pos][0];
      sumYTotal += data[pos][1];
      sumZTotal += data[pos][2];
      if (i >= (15-3)) {
        sumXLastThree += data[pos][0];
        sumYLastThree += data[pos][1];
        sumZLastThree += data[pos][2];
      }
      pos = (pos + 1) % 15;
    }
    sumXTotal /= 15;
    sumYTotal /= 15;
    sumZTotal /= 15;
    sumXLastThree /= 3;
    sumYLastThree /= 3;
    sumZLastThree /= 3;

    Serial.print("Average[15]: ");
    Serial.print("X: "); Serial.print(sumXTotal); Serial.print("  ");
    Serial.print("Y: "); Serial.print(sumYTotal); Serial.print("  ");
    Serial.print("Z: "); Serial.print(sumZTotal); Serial.print("  ");
    Serial.println("uT");
    Serial.print("Average[3]: ");
    Serial.print("X: "); Serial.print(sumXLastThree); Serial.print("  ");
    Serial.print("Y: "); Serial.print(sumYLastThree); Serial.print("  ");
    Serial.print("Z: "); Serial.print(sumZLastThree); Serial.print("  ");
    Serial.println("uT");

    float deltaX = sumXTotal - sumXLastThree; if (deltaX < 0) deltaX = -deltaX;
    float deltaY = sumYTotal - sumYLastThree; if (deltaY < 0) deltaY = -deltaY;
    float deltaZ = sumZTotal - sumZLastThree; if (deltaZ < 0) deltaZ = -deltaZ;
    float deltaSum = deltaX+deltaY+deltaZ;
    Serial.print("Delata[3]: ");
    Serial.print("X: "); Serial.print(deltaX); Serial.print("  ");
    Serial.print("Y: "); Serial.print(deltaY); Serial.print("  ");
    Serial.print("Z: "); Serial.print(deltaZ); Serial.print("  ");
    Serial.print("SUM: "); Serial.print(deltaSum); Serial.print("  ");
    alarm = deltaSum > 20;
    if (alarm) Serial.print("!ALARM!  ");
    Serial.println("uT");

    Serial.println();
    Serial.println();
  }

  if (alarm) {
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
  } else {
    delay(2000);
  }
  delay(2000);

}
