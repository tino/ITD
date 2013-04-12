#include "Accelerometer.h"

Accelerometer accel = Accelerometer();

// this is where we will put our data
int myData = 0;

void setup(){
  // Start up our serial port, we configured our XBEE devices for 38400 bps.
  Serial.begin(9600);
  pinMode(13, OUTPUT);

  accel.begin(0, 1, 2);
  //calibration performed below
  Serial.println("Please place the Accelerometer on a flat\nLevel surface");
  delay(1000);//Give user 2 seconds to comply
  accel.calibrate();
  Serial.println("Calibrated");

  // Magnetic sensor
  pinMode(4, INPUT);
}

void loop(){
  delay(50); //delay for readability
  //reads the values of your accelerometer
  accel.read(5);
  Serial.print(accel._Xgs);
  Serial.print("\t");
  Serial.print(accel._Ygs);
  Serial.print("\t");
  Serial.print(accel._Zgs);
  // and the magnetic
  Serial.print("\t");
  Serial.println(analogRead(4));

  // Serial prints:
  // X  Y  Z  M

  // delay(100);
  // Serial.print(myData);
  // Serial.print(' ');
  // Serial.println(analogRead(0));
}
