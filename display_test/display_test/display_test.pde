import processing.serial.*;

Serial myPort;

int HZ = 20; // 50 millis resolution of arduino
float THRESHOLD = 3.0;
float SHAKE_THRESHOLD_DOWN = 3.0;
float SHAKE_THRESHOLD_UP = -1.5 ;
float BALANCE_SHAKE_THRESHOLD = 3.0;
String message;
String inString;
int state=0;
String inMsg = "";
int[] xQueue = new int[400];
int [] yQueue = new int[400];
int[] magneticQueue = new int[400];
boolean _running = true;
int _orientation = 0;
int _lastUpdateActivation = 0;

void setup() {
  frameRate(HZ);
  size(400, 600);
  println(Serial.list());
  String portName = Serial.list()[0];
  myPort = new Serial(this, portName, 9600);
  delay(1000);
  myPort.clear();
  delay(1000);
}

float toYScreen(float y) {
  return (y * 300) + 10;
}

void draw() {
  if (_running) {
    if (boolean(_orientation)) {
      background(255);
      fill(0);
      stroke(0);
    } else {
      background(0);
      fill(255);
      stroke(255);
    }
    textSize(12);
    text("0", 0, toYScreen(0));
    text("1", 0, toYScreen(1));
//    text(xQueue[xQueue.length-1], 0, 20);
//    text(magneticQueue[magneticQueue.length-1], 0, 40);
//    text(_lastUpdateActivation, 0, 60);
    // line every second
    for (int i=0; i<=400; i++) {
      if (i % HZ == 0) {
        line(i, toYScreen(0)-20, i, toYScreen(0)+20);
      }
    }
    noFill();
    stroke(196, 0, 0);
    beginShape();
    for (int i=0; i<xQueue.length; i++) {
      curveVertex(i, toYScreen(xQueue[i]));
    }
    endShape();
    stroke(104, 175, 244);
    beginShape();
    for (int i=0; i<yQueue.length; i++) {
      curveVertex(i, toYScreen(yQueue[i]));
    }
    endShape();

    if (updateActivated()) {
      fill(255, 255, 255);
      rect(250, 10, 40, 40);
      if (checkForUpdate()) {
        fill(196, 0, 0);
        rect(350, 10, 40, 40);
      }
    } else {
      if (checkForBalanceCheck()) {
        fill(0, 200, 0);
        rect(350, 50, 40, 40);
      }
    }
  }
}

boolean updateActivated() {
  if (_lastUpdateActivation > 0)
    return millis() - _lastUpdateActivation < 3 * 1000;
  return false;
}

void setUpdateActivation() {
  // There should be at least a quarter second of high on the magnetic
  // sensor to be activated
  int items = HZ / 4;
  int sum = 0;
  for(int i=magneticQueue.length-1; i > (magneticQueue.length - items - 1); i--) {
    sum = sum + magneticQueue[i];
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
  for (int i=1; i < HZ/2 + 1; i++) {
    if (xQueue[xQueue.length - i] == yQueue[yQueue.length - i]) {
      sum = sum + xQueue[xQueue.length - i];
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
  float[] diffs = new float[items];
  
  int changes = 0;
  for (int i=1; i<items; i++) {
    if (xQueue[xQueue.length-i] != xQueue[xQueue.length-i-1])
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
  int[] diffs = new int[items];
  
  int changes = 0;
  for (int i=1; i<items; i++) {
    if (xQueue[xQueue.length-i] != xQueue[xQueue.length-i-1])
      changes = changes + 1;
  }

  println(changes);
  
  if (changes == 2) {
    return true;
  } else {
    return false;
  } 
}
  

void serialEvent(Serial p) {
  try {
    inString = p.readStringUntil('\n');
    if (inString!=null) {
      inString = trim(inString);
      String[] values = splitTokens(inString);
      if (values.length == 3) {
        try {
          int x = int(values[0]);
          int y = int(values[1]);
          int m = int(values[2]);
//          int m = int(values[3]);
          pushXQueue(x);
          pushYQueue(y);
          pushMagneticQueue(m);
          setOrientation();
          setUpdateActivation();
        } catch (Exception e) {
          println("Error converting values");
        }

      }
    }
  } catch(Exception e) {
    println("Error reading serial");
  }
}

void keyPressed() {
  if (_running) {
    _running = false;
  } else {
    _running = true;
  }
}


void pushXQueue(int v) {
  int[] newQueue = subset(xQueue, 1);
  int[] newQueue2 = append(newQueue, v);
  xQueue = newQueue2;
}
void pushYQueue(int v) {
  int[] newQueue = subset(yQueue, 1);
  int[] newQueue2 = append(newQueue, v);
  yQueue = newQueue2;
}
void pushMagneticQueue(int v) {
  int[] newQueue = subset(magneticQueue, 1);
  int[] newQueue2 = append(newQueue, v);
  magneticQueue = newQueue2;
}
