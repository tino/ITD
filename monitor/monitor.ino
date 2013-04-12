
// this is where we will put our data
int myData = 0;

void setup(){
  // Start up our serial port, we configured our XBEE devices for 38400 bps.
  Serial.begin(9600);
  pinMode(7, INPUT);
  pinMode(6, INPUT);
}

void loop(){
  delay(50);
  Serial.print(digitalRead(7));
  Serial.print("\t");
  Serial.print(digitalRead(8));
  Serial.print("\t");
  Serial.println(analogRead(7));
}

