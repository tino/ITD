#include <Firmata.h>
// Constants and global vars
// ----------

// Device ID
// Letters A-Z can be used. 0 is reserved for broadcast
#define ID 'U'

//enable to show debugging information about parsing and operations
//not so good on broadcast channels
const int DEBUG = 0;

// Pins
const int MAGNETIC_PIN = 7;
const int TILT_PIN_1 = 7;
const int TILT_PIN_2 = 8;

// Times
const int HZ = 10;
const int UPDATE_ACTIVATION_DURATION = 1 * 1000;
const int UPDATE_WINDOW_DURATION = 10 * 1000;
const int UPDATE_SHAKE_DURATION = 1000 / 2;

// Thresholds
const float THRESHOLD = 3.0;
const float SHAKE_THRESHOLD_DOWN = 3.0;
const float SHAKE_THRESHOLD_UP = -1.5 ;
const float BALANCE_SHAKE_THRESHOLD = 3.0;

// Messaging commands
#define SYN_UPDATE_WINDOW        'A' // Start update sync
#define ACK_UPDATE_WINDOW        'B' // Acknowledge update sync
#define OPEN_UPDATE_WINDOW       'C' // Update window is open
#define SYN_UPDATE_SHAKE         'D' // Start shake sync
#define ACK_UPDATE_SHAKE         'E' // Acknowledge shake sync
#define DO_UPDATE                'F' // Do the update!

// States
#define PASSIVE                  1
#define UPDATE_WINDOW_ACTIVATED  2
#define UPDATE_WINDOW_ACKED      3
#define UPDATE_WINDOW_OPEN       4
#define UPDATE_SHAKE_ACTIVATED   5
#define UPDATE_SHAKE_ACKED       6
#define UPDATE_DONE              7


// Deque's (see bottom for push methods)
const int HISTORY = 4 * HZ; // how many datapoints to rememeber (secs * HZ)
int _xQueue[HISTORY];
int _yQueue[HISTORY];
int _magneticQueue[HISTORY];

// Global variables
int _orientation = 0;
int _state = PASSIVE;
unsigned long _lastUpdateActivation = 0;
unsigned long _updateWindowOpenStart = 0;
unsigned int _updateWindowPartner = 0;
unsigned long _updateShakeTime = 0;


void setup(){
  // Start up our serial port, we configured our XBEE devices for 57600 bps.
  Serial.begin(57600);

  // Setup pins
  pinMode(TILT_PIN_1, INPUT);
  pinMode(TILT_PIN_2, INPUT);
  pinMode(13, OUTPUT);

  if (DEBUG) {
    Serial.print(ID);
    Serial.println(" has entered the building");
  }

}

void loop(){
  static unsigned long _lastHeartBeat = 0;
  static unsigned long _lastLoop = 0;
  static boolean _on = false;

  processSerial();

  if (millis() - _lastLoop > 1000/HZ)  {
    _lastLoop = millis();

    // Update our readings
    int x = digitalRead(TILT_PIN_1);
    int y = digitalRead(TILT_PIN_2);
    int m = analogRead(MAGNETIC_PIN);
    pushQueue(_xQueue, HISTORY, x);
    pushQueue(_yQueue, HISTORY, y);
    pushQueue(_magneticQueue, HISTORY, m);

    // Process readings
    setOrientation();
    setUpdateActivation();

    // Act according to state
    switch (_state) {

      case PASSIVE:
        if (updateActivated()) {
          // We are passive and update was activated, so broadcast
          sendDebug("updateActivated", 1);
          _state = UPDATE_WINDOW_ACTIVATED;
          sendRequest('0', SYN_UPDATE_WINDOW, 'x', 'x');
        }
        if (checkForBalanceCheck()) {
          sendDebug("balance", 1);
          // TODO: LEDS
        }
      break;

      case UPDATE_WINDOW_ACTIVATED:
        // if updateActivation is over reset state
        if (!updateActivated()) _state = PASSIVE;
      break;

      case UPDATE_WINDOW_ACKED:
        // if updateActivation is over reset state and we havent' moved to
        // OPEN_UPDATE_WINDOW in the mean time, reset
        if (!updateActivated()) _state = PASSIVE;
        // TODO: close window, so the other side is closed for sure as well?
      break;

      case UPDATE_WINDOW_OPEN:
        // If the time window has closed, reset state to PASSIVE
        if (millis() - _updateWindowOpenStart > UPDATE_WINDOW_DURATION) {
          _updateWindowPartner = 0;
          _state = PASSIVE;
          break;
        }

        // Check for an update shake
        if (checkForUpdate()) {
          // send message to window partner
          sendRequest(_updateWindowPartner, SYN_UPDATE_SHAKE, 0, 0);
          _state = UPDATE_SHAKE_ACTIVATED;
        }
      break;

      case UPDATE_SHAKE_ACTIVATED:
        // reset if it takes too much time
        if (millis() - _updateShakeTime > UPDATE_SHAKE_DURATION) {
          if (millis() - _updateWindowOpenStart > UPDATE_WINDOW_DURATION) {
            _state = PASSIVE;
            _updateWindowPartner = 0;
          } else {
            _state = UPDATE_WINDOW_OPEN;
          }
        }
      break;

      case UPDATE_SHAKE_ACKED:
        // reset if it takes too much time
        if (millis() - _updateShakeTime > UPDATE_SHAKE_DURATION) {
          if (millis() - _updateWindowOpenStart > UPDATE_WINDOW_DURATION) {
            _state = PASSIVE;
            _updateWindowPartner = 0;
          } else {
            _state = UPDATE_WINDOW_OPEN;
          }
        }
      break;
    }
  }

  // Heartbeats and DEBUG
  if (millis() - _lastHeartBeat > 2000) {
    // sendDebug("Alive", 1);
    // sendDebug("orientation", _orientation);
    sendDebug("state", _state);
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


boolean updateActivated() {
  if (_lastUpdateActivation > 0)
    return (millis() - _lastUpdateActivation) < UPDATE_WINDOW_DURATION;
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

void doShake() {
  // TODO: update the numbers!
}

// Helper function to use arrays as deque's
// push v to array[0] and shift every thing up 1, last items slides of
void pushQueue(int array[], int size, int v) {
  for (int i = 0; i < size-1; i++) {
    array[i+1] = array[i];
  }
  array[0] = v;

}


// Messaging

// 01: AF
// 2: to
// 3: from
// 4: operation
// 5: operand1
// 6: operand2 MSB
// 7: operand2 LSB
// 89: FA

// 01: SR
// 2: to
// 3: from
// 4: secondary res
// 5: primary res MSB
// 6: primary res LSB
// 78: RS


void execute(unsigned char from, unsigned char operation, unsigned char operand1, int operand2){
  if (DEBUG){
    Serial.println("----");
    Serial.print(" from=");
    Serial.print(from);
    Serial.print(" operation=");
    Serial.print(operation);
    Serial.print(" operand1=");
    Serial.print(operand1);
    Serial.print(" operand2=");
    Serial.println(operand2);
    Serial.println("----");
  }

  switch (operation) {
    case SYN_UPDATE_WINDOW:
      if (DEBUG) {
        Serial.print("SYN_UPDATE_WINDOW; state: ");
        Serial.println(_state);
      }
      // If we are in UPDATE_WINDOW_ACTIVATED state as well, we respond
      if (_state == UPDATE_WINDOW_ACTIVATED) {
        // We are ready to do an update, let's see who it is and reply
        sendRequest(from, ACK_UPDATE_WINDOW, '0', '0');
        _state = UPDATE_WINDOW_ACKED;
      }
      // We are either passive, or already in conversation with someone, so
      // ignore
    break;

    case ACK_UPDATE_WINDOW:
      // Someone responds to an SYN_UPDATE_WINDOW request. Are we still
      // in the UPDATE_WINDOW_ACTIVATED state?
      if (DEBUG) {
        Serial.print("ACK_UPDATE_WINDOW; state: ");
        Serial.println(_state);
      }

      if (_state == UPDATE_WINDOW_ACKED || _state == UPDATE_WINDOW_ACTIVATED) {
        // Oke, activation window is open
        sendRequest(from, OPEN_UPDATE_WINDOW, '0', '0');
        _state = UPDATE_WINDOW_OPEN;
        _updateWindowPartner = from;
        _updateWindowOpenStart = millis();
      }
    break;

    case OPEN_UPDATE_WINDOW:
      // Response to ACK_UPDATE_WINDOW, so we should be in UPDATE_WINDOW_ACKED state
      if (DEBUG) {
        Serial.print("OPEN_UPDATE_WINDOW; state: ");
        Serial.println(_state);
      }
      if (_state != UPDATE_WINDOW_ACKED) return;
      // Window is opened on the other side, open here as well
      _state = UPDATE_WINDOW_OPEN;
      _updateWindowPartner = from;
      _updateWindowOpenStart = millis();
    break;

    case SYN_UPDATE_SHAKE:
      // window still open?
      if (_state == UPDATE_WINDOW_OPEN || _state == UPDATE_SHAKE_ACTIVATED) {
        // TODO: do we need to check from == _updateWindowPartner?
        sendRequest(from, ACK_UPDATE_SHAKE, _orientation, 0);
        _state = UPDATE_SHAKE_ACKED;
      }
    break;

    case ACK_UPDATE_SHAKE:
      // Was ours within the limit?
      if (_state == UPDATE_SHAKE_ACTIVATED) {
        // We both had a shake!
        // are the orientations different?
        if (_orientation != operand1) {
          sendRequest(from, DO_UPDATE, _orientation, 0);
          doShake();
        }
      }

  }
}

void sendRequest(unsigned char to, unsigned char operation, unsigned char operand1, unsigned char operand2) {
  // Send out a message. to is the ID of the targeted machine
  // operand1 cannot be greater than 255
  // operand2 can be, and will be send as a MSB and LSB
  Serial.print("AF");
  Serial.write(to);
  Serial.write(ID);
  Serial.write(operation);
  Serial.write(operand1);
  Serial.write(highByte(operand2));
  Serial.write(lowByte(operand2));
  Serial.print("FA");
}

// void sendResponse(byte to, byte res, byte res2) {
//   // Reply to a message. to is the ID of the targeted machine
//   Serial.print("SR");
//   Serial.write(to);
//   Serial.write(ID);
//   Serial.write(res2);           //secondary result, one byte
//   Serial.write(highByte(res));  //primary result, two bytes
//   Serial.write(lowByte(res));
//   Serial.print("RS");
// }

void processSerial() {
  if (!Serial.available()) return;

  static short int state = 0;
  static unsigned char from = 255;
  static unsigned char operation = 255;
  static unsigned char operand1 = 255;
  static int operand2 = -1;

  char c = Serial.read();

  if (DEBUG > 1){
    digitalWrite(13,HIGH);
    delay(5);
    digitalWrite(13,LOW);
    Serial.print(state);
    Serial.print(" ");
    Serial.println(c);
  }

  switch(state) {

    case 0:
      // A
      if (c == 'A')
        state = 1;
      else
        state = 0; // reset state
    break;

    case 1:
      // F
      if (c == 'F') {
        state=2;
      }
      else if (c =='A') {
        state = 1; // stay right were you are
      }
      else {
        state = 0;
      }
    break;

    case 2:
      // Target/to
      // Is the message for us or a broadcast? If not ignore the rest
      if (c == ID || c == '0') {
        state = 3;
      }
    break;

    case 3:
      // From
      from = c;
      state = 4;
    break;

    case 4:
      operation = c;
      state = 5;
      break;

    case 5: //read first operand in
      operand1 = c;
      state = 6;
    break;

    case 6: //read first byte of second operand
      operand2 = c;
      state = 7;
    break;

    case 7: //read second byte of second operand
      operand2 = word(operand2, c);
      state = 8;
    break;

    case 8:
      if (c == 'F')
        state = 9;
      else
        state = 0;
    break;

    case 9:
      if (c != 'A') {
        state = 0;
        return;
      }

      execute(from, operation, operand1, operand2);
      state = 0;
    break;

    default:
    break;

  }
}


void sendDebug(char key[], int value) {
  if (DEBUG == 0) return;
  Serial.print("<");
  Serial.print(ID);
  Serial.print(":");
  Serial.print(key);
  Serial.print(":");
  Serial.print(value);
  Serial.println(">");
}

