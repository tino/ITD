#include <Firmata.h>
// Constants and global vars
// ----------

// Device ID
// Letters A-Z can be used. 0 is reserved for broadcast
#define ID 'Y'

// Pins
const int MAGNETIC_PIN = 7;
const int TILT_PIN_1 = 7;
const int TILT_PIN_2 = 8;

// Thresholds
const int HZ = 10;
const int UPDATE_ACTIVATION_WINDOW = 3 * 1000; // 10 seconds
const int UPDATE_WINDOW = 10 * 1000; // 10 seconds
float THRESHOLD = 3.0;
float SHAKE_THRESHOLD_DOWN = 3.0;
float SHAKE_THRESHOLD_UP = -1.5 ;
float BALANCE_SHAKE_THRESHOLD = 3.0;

// Messaging commands
#define UPDATE_SYN        'A' // Start update sync
#define UPDATE_ACK        'B' // Acknowledge update sync
#define UPDATE_OPEN       'C' // Update window is open

// States
#define PASSIVE           1
#define UPDATE_ACTIVATED  2
#define UPDATE_ACKED      3
#define UPDATE_OPENED     4


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


// Afro stuff
#define REGISTERSIZE 10
long int reg[REGISTERSIZE];

//enable to show debugging information about parsing and operations
//not so good on broadcast channels
boolean debug=false;

//debug mode will toggle this pin regularly, to let you know the parser is working
int beeperPin=12;

//Afro will delay this much before answering a broadcast message, to make collisions less likely
int broadcastDelay = (ID*79)%255;

unsigned char res0='Z'; //the secondary result from the last query
int res=0;              //the result from the last query


void setup(){
  // Start up our serial port, we configured our XBEE devices for 9600 bps.
  Serial.begin(57600);
  // Initiate Firmata
  // Firmata.setFirmwareVersion(1, 1);
  // Firmata.attach(START_SYSEX, sysexCallback);
  // Firmata.begin(115200);

  // Setup pins
  pinMode(TILT_PIN_1, INPUT);
  pinMode(TILT_PIN_2, INPUT);
  pinMode(13, OUTPUT);

  Serial.write(100);
  Serial.write(0x00);
  Serial.write(100);

}

void loop(){
  static unsigned long _lastHeartBeat = 0;
  static unsigned long _lastLoop = 0;
  static boolean _on = false;

  if (millis() - _lastLoop > 1000/HZ)  {
    _lastLoop = millis();
    processSerial();
  }
  //   int x = digitalRead(TILT_PIN_1);
  //   int y = digitalRead(TILT_PIN_2);
  //   int m = analogRead(MAGNETIC_PIN);
  //   pushQueue(_xQueue, HISTORY, x);
  //   pushQueue(_yQueue, HISTORY, y);
  //   pushQueue(_magneticQueue, HISTORY, m);

  //   setOrientation();
  //   setUpdateActivation();

  //   switch (_state) {

  //     case PASSIVE:
  //       if (updateActivated()) {
  //         // We are passive and update was activated, so broadcast
  //         sendDebug("updateActivated", 1);
  //         _state = UPDATE_ACTIVATED;
  //         sendRequest(0, UPDATE_SYN, 'x', 'x');
  //       }
  //       if (checkForBalanceCheck()) {
  //         sendDebug("balance", 1);
  //         // TODO: LEDS
  //       }
  //     break;

  //     case UPDATE_ACTIVATED:
  //       // if updateActivation is over reset state
  //       if (!updateActivated()) _state = PASSIVE;
  //     break;

  //     case UPDATE_ACKED:
  //       // if updateActivation is over reset state and we havent' moved to
  //       // UPDATE_OPEN in the mean time, reset
  //       if (!updateActivated()) _state = PASSIVE;
  //       // TODO: close window, so the other side is closed for sure as well?
  //     break;

  //     case UPDATE_OPENED:
  //       // If the time window has closed, reset state to PASSIVE
  //       if (millis() - _updateWindowOpenStart > UPDATE_WINDOW) _state = PASSIVE;
  //     break;
  //   }
  // }

  // Heartbeats and debug
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



void processSerial() {
  if (Serial.available()>0){ //characters available for processing
    processBuffer();
  }
}

void sendDebug(char key[], int value) {
  Serial.print("<");
  Serial.print(ID);
  Serial.print(":");
  Serial.print(key);
  Serial.print(":");
  Serial.print(value);
  Serial.println(">");
}


boolean updateActivated() {
  if (_lastUpdateActivation > 0)
    return (millis() - _lastUpdateActivation) < UPDATE_ACTIVATION_WINDOW;
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

void processBuffer() {
  static short int state = 0;
  static unsigned char from = 255;
  static unsigned char operation = 255;
  static unsigned char operand1 = 255;
  static int operand2 = -1;
  char c;
  c= Serial.read();
  if (debug){
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

void execute(unsigned char from, unsigned char operation, unsigned char operand1, int operand2){
  if (debug){
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
    case UPDATE_SYN:
      if (operand1 > 122) operand1 = '@';
      sendRequest(from, UPDATE_SYN, operand1+1, 0);

    break;
    // case UPDATE_SYN:
    //   if (debug) {
    //     Serial.print("UPDATE_SYN; state: ");
    //     Serial.println(_state);
    //   }
    //   // If we are in UPDATE_ACTIVATED state as well, we respond
    //   if (_state == UPDATE_ACTIVATED) {
    //     // We are ready to do an update, let's see who it is and reply
    //     sendRequest(from, UPDATE_ACK, 0);
    //     _state = UPDATE_ACKED;
    //   }
    //   // We are either passive, or already in conversation with someone, so
    //   // ignore
    // break;

    // case UPDATE_ACK:
    //   // Someone responds to an UPDATE_SYN request. Are we still
    //   // in the UPDATE_ACTIVATED state?
    //   if (debug) {
    //     Serial.print("UPDATE_ACK; state: ");
    //     Serial.println(_state);
    //   }

    //   if (_state != UPDATE_ACTIVATED) return;
    //   // Oke, activation window is open
    //   _state = UPDATE_OPENED;
    //   _updateWindowOpenStart = millis();
    //   sendRequest(from, UPDATE_OPEN, 0, 0);
    // break;

    // case UPDATE_OPEN:
    //   // Response to UPDATE_ACK, so we should be in UPDATE_ACKED state
    //   if (debug) {
    //     Serial.print("UPDATE_OPEN; state: ");
    //     Serial.println(_state);
    //   }
    //   if (_state != UPDATE_ACKED) return;
    //   // Window is opened on the other side, open here as well
    //   _state = UPDATE_OPENED;
    //   _updateWindowOpenStart = millis();
  }
}



