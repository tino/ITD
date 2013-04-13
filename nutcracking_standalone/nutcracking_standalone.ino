// Constants and global vars
// ----------

// Device ID
#define ID 1

// Pins
const int MAGNETIC_PIN = 7;
const int TILT_PIN_1 = 7;
const int TILT_PIN_2 = 8;

// Thresholds
const int HZ = 20; // 50 millis resolution of arduino
float THRESHOLD = 3.0;
float SHAKE_THRESHOLD_DOWN = 3.0;
float SHAKE_THRESHOLD_UP = -1.5 ;
float BALANCE_SHAKE_THRESHOLD = 3.0;

// Deque's (see bottom for push methods)
#define HISTORY 100 // how many datapoints to rememeber
int _xQueue[HISTORY];
int _yQueue[HISTORY];
int _magneticQueue[HISTORY];

// Global variables
int _orientation = 0;
int _lastUpdateActivation = 0;
int _lastHeartBeat = 0;
int _lastLoop = 0;
int _on = false;

void setup(){
  // Start up our serial port, we configured our XBEE devices for 9600 bps.
  Serial.begin(115200);
  pinMode(TILT_PIN_1, INPUT);
  pinMode(TILT_PIN_2, INPUT);
  pinMode(13, OUTPUT);
  int test[4];
  int sum = 0;
  for (int i=0; i<HISTORY; i++) {
    sum += _magneticQueue[i];
  }
    Serial.println(sum);
}

void loop(){
  if (millis() - _lastLoop > 1000/HZ)  {
    _lastLoop = millis();

    int x = digitalRead(TILT_PIN_1);
    int y = digitalRead(TILT_PIN_2);
    int m = analogRead(MAGNETIC_PIN);
    pushQueue(_xQueue, HISTORY, x);
    pushQueue(_yQueue, HISTORY, y);
    pushQueue(_magneticQueue, HISTORY, m);
    // Serial.print(_xQueue[0]);
    // Serial.print("\t");
    // Serial.print(_yQueue[0]);
    // Serial.print("\t");
    // Serial.println(_magneticQueue[0]);
    setOrientation();
    setUpdateActivation();
    // sendData("orientation", _orientation);
    sendData("lastupdate", _lastUpdateActivation);

    if (updateActivated()) {
      if (checkForUpdate()) {
        // TODO: make sure this happens once per updateActivated cycle
        sendData("update", 1);
      }
    }
    else {
      if (checkForBalanceCheck()) {
        sendData("balance", 1);
      }
    }
  }
  if (millis() - _lastHeartBeat > 1000) {
    sendData("Alive", 1);
    _lastHeartBeat = millis();
    if (_on) {
      digitalWrite(13, 0);
      _on = false;
    } else {
      digitalWrite(13, 1);
      _on = true;
    }
  }
}

void sendData(char key[], int value) {
  Serial.print("<<");
  Serial.print(ID);
  Serial.print(":");
  Serial.print(key);
  Serial.print(":");
  Serial.print(value);
  Serial.println(">>");
}


boolean updateActivated() {
  if (_lastUpdateActivation > 0)
    return (millis() - _lastUpdateActivation) < 3 * 1000;
  return false;
}

void setUpdateActivation() {
  // There should be at least a quarter second of high on the magnetic
  // sensor to be activated
  int items = HZ / 4;
  int sum = 0;
  for(int i=0; i < items; i++) {
    sum += _magneticQueue[i];
  }
  Serial.println(sum / items);
  if (sum / items > 950) {
    _lastUpdateActivation = millis();
  }
}

void setOrientation() {
  // see http://www.parallax.com/portals/0/downloads/docs/prod/sens/28036-4DirectionalTiltSensor-v1.0.pdf
  // 0 / 0 is palm down
  // 1 / 1 is palm down, sorta... :)
  // average over the half second
  int sum = 0;
  for (int i=1; i < HZ/2 + 1 ; i++) {
    if (_xQueue[i] == _yQueue[i]) {
      sum = sum + _xQueue[i];
    }
  }
  if ((sum / float(HZ/2)) > 0.8) {
    _orientation = 1;
  } else if ((sum / float(HZ/2)) < 0.1) {
    _orientation = 0;
  } // else don't change
}

boolean checkForBalanceCheck() {
  // Check for continuous peaks in the last 2 seconds. A positive, negative and positive peak
  // mean a shake. Peaks are counted as sign changes.
  int items = 2 * HZ;
  float diffs[items];

  int changes = 0;
  for (int i=1; i<items; i++) {
    if (_xQueue[HISTORY-i] != _xQueue[HISTORY-i-1])
      changes = changes + 1;
  }
//  print("changes: ");
//  print(changes);
//  print(" items: ");
//  println(items);
  if (changes > 0 && (items / changes < 4)) {
    return true;
  } else {
    return false;
  }
}

boolean checkForUpdate() {
  // Check for one single peak in the last second
  int items = HZ;
  int diffs[items];

  int changes = 0;
  for (int i=1; i<items; i++) {
    if (_xQueue[HISTORY-i] != _xQueue[HISTORY-i-1])
      changes = changes + 1;
  }

  if (changes == 2) {
    return true;
  } else {
    return false;
  }
}

// Helper functions to create deque's
void pushQueue(int array[], int size, int v) {
  for (int i = 0; i < size-1; i++) {
    array[i+1] = array[i];
  }
  array[0] = v;

}
// void pushXQueue(int v) {
//   int newQueue[HISTORY-1] = subset(_xQueue, 1);
//   int newQueue2[HISTORY] = append(newQueue, v);
//   _xQueue = newQueue2;
// }
// void pushYQueue(int v) {
//   int newQueue[HISTORY-1] = subset(_yQueue, 1);
//   int newQueue2[HISTORY] = append(newQueue, v);
//   _yQueue = newQueue2;
// }
// void pushMQueue(int v) {
//   int newQueue[HISTORY-1] = subset(_magneticQueue, 1);
//   int newQueue2[HISTORY] = append(newQueue, v);
//   _magneticQueue = newQueue2;
// }
