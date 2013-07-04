#define ID 'Y'
// ^ Device ID
// Letters A-Z can be used. 0 is reserved for broadcast

#include <Firmata.h>
#include <Tlc5940.h>
#include <SimpleTimer.h>

// Constants and global vars
// ----------

// enable to show debugging information about parsing and operations
// not so good on broadcast channels. Not a constant so we can switch it live
// 0 = off
// 1 = lowest debug level
// the higher the number, the more verbose the logging becomes
// double digit debug levels can be used for specific types, so setting debug
// to 23 will show everything of level 1 + everything 21 - 23
int DEBUG = 2;  // FIXME: devices behave erronous when starting with DEBUG = 0...

// Pins
const int MAGNETIC_PIN = 6;

// Times
const int HZ = 20;
const int HEARTBEAT = 1 * 1000;
const int UPDATE_ACTIVATION_DURATION = 1 * 1000;
const int UPDATE_WINDOW_DURATION = 1 * 1000;
const int OUTPUT_TEST_DURATION = 2 * 1000;


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
#define ABORT                    'Z' // Abort window and shake
#define SET_DEBUG_LEVEL          'T' // Set debug level
#define OUTPUT_TEST              'O' // Turn all outputs on for 2 secs
#define PRESENT_AT_COUNTER       'Q' // Counter tells somebody is present
#define COUNTER_ACTIONS_DONE     'V' // Updating device at counter is done
#define LEFT_COUNTER             'W' // Device broke contact with counter

// States
#define PASSIVE                  1
#define UPDATE_WINDOW_ACTIVATED  2
#define UPDATE_WINDOW_ACKED      3
#define UPDATE_WINDOW_OPEN       4
#define UPDATE_SHAKE_ACTIVATED   5
#define UPDATE_SHAKE_ACKED       6
#define UPDATE_DONE              7
#define AT_COUNTER               8

// Deque's (see bottom for push methods)
const int HISTORY = 4 * HZ; // how many datapoints to rememeber (secs * HZ)
int _magneticQueue[HISTORY];

// Global variables
int _state = PASSIVE;
int _lastState = PASSIVE;
unsigned long _lastUpdateActivation = 0;
boolean _outputBlocked = false;

// Update window global vars
char _updateWindowPartner = '0';
unsigned long _updateWindowOpenStart = 0;

SimpleTimer timer;


void setup(){
  // Start up our serial port, we configured our XBEE devices for 57600 bps.
  Serial.begin(57600);

  // Setup TLC that drives the LEDS; with all LEDS off
  Tlc.init(0);

  // Setup pins
  pinMode(MAGNETIC_PIN, INPUT);
  // Turn up the internal pullup resistor
  digitalWrite(MAGNETIC_PIN, HIGH);

  externalMonitor("entrance", ID);
  sendDebug("entrance", ID);

  allLedsOn();
  delay(100);
  allLedsOff();
}

void loop(){
  static unsigned long _lastHeartBeat = 0;
  static unsigned long _lastLoop = 0;

  processSerial();
  timer.run();

  // Main processing loop. Should only run if we are not outputting things to
  // the user, and only HZ times per second.
  if (millis() - _lastLoop > 1000/HZ)  {

    _lastLoop = millis();

    // Update our readings
    int m = digitalRead(MAGNETIC_PIN);
    pushQueue(_magneticQueue, HISTORY, m);

    // Process readings
    setUpdateActivation();

    // Act according to state
    switch (_state) {

      case PASSIVE:
        // Heartbeats and DEBUG
        if (updateActivated()) {
          // We are passive someone showed at the counter, so broadcast
          // sendDebug("counterActivated", 1);
          _state = UPDATE_WINDOW_ACTIVATED;
          sendRequest('0', SYN_UPDATE_WINDOW, 0, 0);
        }
      break;

      case UPDATE_WINDOW_ACTIVATED:
        // if updateActivation is over reset state
        if (!updateActivated()) {
          closeUpdateWindow();
        }
      break;

      case UPDATE_WINDOW_ACKED:
        // if updateActivation is over reset state and we havent' moved to
        // OPEN_UPDATE_WINDOW in the mean time, reset
        if (!updateActivated()) closeUpdateWindow();
      break;

      case UPDATE_WINDOW_OPEN:
        // If the time window has closed, reset state to PASSIVE
        if (millis() - _updateWindowOpenStart > UPDATE_WINDOW_DURATION and not updateActivated()) {
          closeUpdateWindow();
          break;
        }
      break;
    }
  }
  if (millis() - _lastHeartBeat > HEARTBEAT) {
    _lastHeartBeat = millis();

    sendDebug("magnet", digitalRead(MAGNETIC_PIN));
    // sendDebug("magnet", digitalRead);
    externalMonitor("state", _state);

    // lightHeartBeat();

  }
}

//////////////////////////////////////
// ACTION FUNCTIONS
//////////////////////////////////////

boolean updateActivated() {
  if (_lastUpdateActivation > 0)
    return (millis() - _lastUpdateActivation) < UPDATE_ACTIVATION_DURATION;
  return false;
}

void setUpdateActivation() {
  // There should be at least a quarter second of low on the magnetic
  // sensor to be activated
  int items = HZ / 4;
  int sum = 0;
  for(int i=0; i < items; i++) {
    sum += _magneticQueue[i];
  }
  if (sum / float(items) > 0.9) {
    _lastUpdateActivation = millis();
  }
}



void openUpdateWindow(char with) {
  _state = UPDATE_WINDOW_OPEN;
  _updateWindowPartner = with;
  _updateWindowOpenStart = millis();
  sendRequest(_updateWindowPartner, PRESENT_AT_COUNTER, 0, _updateWindowPartner);
  externalMonitor("present_at_counter", _updateWindowPartner);
}

void closeUpdateWindow() {
  sendRequest(_updateWindowPartner, LEFT_COUNTER, 0, _updateWindowPartner);
  externalMonitor("left_counter", _updateWindowPartner);
  _state = PASSIVE;
  _updateWindowPartner = 0;
  _updateWindowOpenStart = 0;
}

//////////////////////////////////////
// OUTPUT FUNCTIONS
//////////////////////////////////////

// TODO
void showCounterActionsDone () {

}

// turn on the leds indicated by 1's in the 8-bit leds byte
// to turn on leds higher than 7, give in startAt = 8
// brightness can be adjusted by passing a float 0 - 1
void updateLeds(byte leds) {
  updateLeds(leds, 0);
}
void updateLeds(byte leds, int startAt) {
  updateLeds(leds, startAt, 1);
}
void updateLeds(byte leds, int startAt, float brightness) {
  sendDebug("updateLeds", leds, 51);
  // Due to a fuckup with the order of the balance leds, we need to invert them
  // when the ID is even.
  if (int(ID) % 2 == 0 and startAt == 0) {
    byte reversed = B00000000;
    for (int i=0; i<8; i++) {
      if (leds & (1 << (7-i))) {
        reversed |= 1 << i;
      }
    }
    leds = reversed;
    sendDebug("led order swapped", leds, 52);
  }

  for (byte x=0; x<8; x++) {
    if (leds & (1 << x)) {
      sendDebug("Turning on led", x + startAt, 53);
      // Led should be on
      Tlc.set(x + startAt, 4095 * brightness);
    } else {
      sendDebug("Turning off led", x + startAt, 53);
      Tlc.set(x + startAt, 0);
    }
  }
  Tlc.update();
}

void allLedsOn() {
  byte leds = B11111111;
  updateLeds(leds);
  leds = B11111111;
  updateLeds(leds, 8);
}
void allLedsOff() {
  _outputBlocked = false;
}

// Test all outputs.
// Set all leds on, and turn on the vibrator
void outputTest() {
  _outputBlocked = true;
  allLedsOn();

  timer.setTimeout(OUTPUT_TEST_DURATION, allLedsOff);
}

//////////////////////////////////////
// HELPER FUNCTIONS
//////////////////////////////////////

// Helper function to use arrays as deque's
// push v to array[0] and shift every thing up 1, last items slides of
void pushQueue(int array[], int size, int v) {
  for (int i = size-1; i > 0; i--) {
    array[i] = array[i-1];
  }
  array[0] = v;

}

//////////////////////////////////////
// MESSAGING FUNCTIONS
//////////////////////////////////////

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

// Process the incomming message
void execute(unsigned char from, unsigned char operation, unsigned char operand1, int operand2){
  if (DEBUG >= 2){
    Serial.println("----");
    Serial.print(ID);
    Serial.print("(");
    Serial.print(_state);
    Serial.print(") received from ");
    Serial.print(char(from));
    Serial.print(": operation=");
    Serial.print(char(operation));
    Serial.print(" operand1=");
    Serial.print(operand1);
    Serial.print(" operand2=");
    Serial.println(operand2);
    Serial.println("----");
  }
  sendDebug("execute", from);

  switch (operation) {
    case SYN_UPDATE_WINDOW:
      sendDebug("received SYN", _state);
      // If we are in UPDATE_WINDOW_ACTIVATED state as well, we respond
      if (_state == UPDATE_WINDOW_ACTIVATED) {
        // We are ready to do an update, let's see who it is and reply
        sendRequest(from, ACK_UPDATE_WINDOW, 0, 0);
        _updateWindowPartner = from;
        _state = UPDATE_WINDOW_ACKED;
      }
      // We are either passive, or already in conversation with someone, so
      // ignore
    break;

    case ACK_UPDATE_WINDOW:
      // Someone responds to an SYN_UPDATE_WINDOW request. Are we still
      // in the UPDATE_WINDOW_ACTIVATED state?
      sendDebug("received ACK", _state);
      if (_state == UPDATE_WINDOW_ACKED || _state == UPDATE_WINDOW_ACTIVATED) {
        // Oke, activation window is open
        sendRequest(from, OPEN_UPDATE_WINDOW, 0, 0);
        openUpdateWindow(from);
      } else {
        sendDebug("wrong state", _state);
      }
    break;

    case OPEN_UPDATE_WINDOW:
      sendDebug("received OPEN", _state);
      // Response to ACK_UPDATE_WINDOW, so we should be in UPDATE_WINDOW_ACKED state
      if (_state != UPDATE_WINDOW_ACKED) return;
      // Window is opened on the other side, open here as well
      openUpdateWindow(_updateWindowPartner);
    break;

    case COUNTER_ACTIONS_DONE:
      // Central system has updated the one at the counter
      showCounterActionsDone();
    break;

    case ABORT:
      // Abort
      closeUpdateWindow();
    break;

    case SET_DEBUG_LEVEL:
      DEBUG = operand2;
      externalMonitor("Debug level set to", DEBUG);
    break;

    case OUTPUT_TEST:
      outputTest();
      sendDebug("All outputs high for 2 secs", 1);
    break;

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
      operand2 = word(c, operand2);
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

//////////////////////////////////////
// DEBUG FUNCTIONS
//////////////////////////////////////

void lightHeartBeat() {
  // Only show heartbeat led if output is not blocked
  if (!_outputBlocked) {
    updateLeds(B00011000, 0, 0.5);
    timer.setTimeout(100, dimHeartBeat);
  }
}

void dimHeartBeat() {
  if (!_outputBlocked) {
    updateLeds(B00000000);
  }
}

void sendDebug(char key[], int value) {
  // Debug with level 1
  sendDebug(key, value, 1);
}
void sendDebug(char key[], int value, int debugLevel) {
  int DEBUG_STREAM = 0;
  int DEBUG_LEVEL = 0;
  int debugStream = 0;

  if (DEBUG > 10) {
    DEBUG_STREAM = DEBUG / 10;
    DEBUG_LEVEL = DEBUG % 10;
  } else {
    DEBUG_LEVEL = DEBUG;
  }

  if (debugLevel > 10) {
    debugStream = debugLevel / 10;
    debugLevel = debugLevel % 10;
  }

  if (debugLevel > DEBUG_LEVEL) return;
  if (debugStream == 0 || debugStream == DEBUG_STREAM) {
    Serial.print("<");
    Serial.print(ID);
    Serial.print(":");
    Serial.print(key);
    Serial.print(":");
    Serial.print(value);
    Serial.print(":");
    Serial.print(millis());
    Serial.println(">");
  }
}

void externalMonitor(char key[], int value) {
  Serial.print("<<");
  Serial.print(ID);
  Serial.print(":");
  Serial.print(key);
  Serial.print(":");
  Serial.print(value);
  Serial.print(":");
  Serial.print(millis());
  Serial.println(">>");
}
