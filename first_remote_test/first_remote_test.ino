
// this is where we will put our data
int myData = 0;

void setup(){
  // Start up our serial port, we configured our XBEE devices for 38400 bps.
  Serial.begin(9600);
  pinMode(13, OUTPUT);
}

void loop(){
  if (Serial.available() > 0) {
    myData = Serial.parseInt();

  }
  if (myData < 800 && analogRead(0) < 800) {
    digitalWrite(13, 1);
  }
  else {
    digitalWrite(13, 0);
  }
  delay(100);
  Serial.print(myData);
  Serial.print(' ');
  Serial.println(analogRead(0));
}
