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
float[] rawQueue = new float[400];
float[] avgQueue = new float[400];
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
  return (y + 10)*30;
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
    text("2.5", 0, toYScreen(2.5));
    text("5", 0, toYScreen(5));
    text("-1", 0, toYScreen(-1));
//    text(rawQueue[rawQueue.length-1], 0, 20);
//    text(magneticQueue[magneticQueue.length-1], 0, 40);
//    text(_lastUpdateActivation, 0, 60);
    for (int i=0; i<=400; i++) {
      if (i%20 == 0) {
        line(i, toYScreen(0)-20, i, toYScreen(0)+20);
      }
    }
    noFill();
    stroke(196, 0, 0);
    beginShape();
    for (int i=0; i<rawQueue.length; i++) {
      curveVertex(i, toYScreen(rawQueue[i]));
    }
    endShape();
    stroke(104, 175, 244);
    beginShape();
    for (int i=0; i<avgQueue.length; i++) {
      curveVertex(i, toYScreen(avgQueue[i]));
    }
    endShape();
    setOrientation();
    setUpdateActivation();
//    println(_orientation

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
  // Calculate the averages of the rawQueue over the last half second
  // If the two last averages are above / below a certain range, it 
  // determines up or down state.
  // data of last half second is 1/2 * HZ values from rawQueue
  int items = HZ / 2;
  float fhalf = 0; 
  float lhalf = 0;
  for(int i=rawQueue.length-1; i > (rawQueue.length - items - 1); i--) {
    if (i > (rawQueue.length - 1 - items / 2)) {
      // first half
      fhalf = fhalf + rawQueue[i];
    } else {
      lhalf = lhalf + rawQueue[i];
    }
  }
  float favg = fhalf / (items / 2);
  float lavg = lhalf / (items / 2);
  if ((favg > THRESHOLD) && (lavg > THRESHOLD)) {
    _orientation = 1;
  } else if ((favg < THRESHOLD) && (lavg < THRESHOLD)) {
    _orientation = 0;
  } // else don't change the orientation
//  print("lavg: ");
//  print(lavg);
//  print(" favg: ");
//  print(favg);
//  print(" or: ");
//  println(_orientation);
}

boolean checkForBalanceCheck() {
  // Check for continuous peaks in the last 2 seconds. A positive, negative and positive peak
  // mean a shake. Peaks are counted as sign changes. 
  int items = 2 * HZ;
  float[] diffs = new float[items];
  
  for (int i=0; i<items; i++) {
    float diff = rawQueue[rawQueue.length-1-i] - avgQueue[avgQueue.length-1-i];
    
    if (abs(diff) > BALANCE_SHAKE_THRESHOLD) {
      diffs[i] = diff;
    }
  }
  
  // Check for sign changes within non-zero values
  int changes = 0;
  float last = diffs[0];
  for (int i=0; i<items; i++) {
    if (last != 0.0) {
//      if (diffs[i] != 0.0) {
        if ((abs(last - diffs[i]) == last) || (last < 0.0) ^ (diffs[i] < 0.0)) {
           changes = changes + 1;
           // print(last); print(" "); print(diffs[i]);print(" ");
        }
        last = diffs[i];
//      }
    } else {
      last = diffs[i];
    }
    
  }
  println(diffs);
//  println(changes);
  
  if (changes >= 5) {
    return true;
  } else {
    return false;
  } 
//  print("lavg: ");
//  print(lavg);
//  print(" favg: ");
//  print(favg);
//  print(" or: ");
}

boolean checkForUpdate() {
  // Check for peaks in the last seconds. A positive, negative and positive peak
  // mean a shake. Peaks are counted as sign changes. 
  int items = HZ;
  float[] diffs = new float[HZ];
  
  for (int i=0; i<HZ; i++) {
    float diff = rawQueue[rawQueue.length-1-i] - avgQueue[avgQueue.length-1-i];
    
    if (diff > SHAKE_THRESHOLD_DOWN) {
      diffs[i] = diff;
    }
    if (diff < SHAKE_THRESHOLD_UP) {
      diffs[i] = diff;
    }
  }
  
  // Check for sign changes within non-zero values
  int changes = 0;
  float last = diffs[0];
  for (int i=0; i<HZ; i++) {
    if (last != 0.0) {
//      if (diffs[i] != 0.0) {
        if ((abs(last - diffs[i]) == last) || (last < 0.0) ^ (diffs[i] < 0.0)) {
           changes = changes + 1;
           // print(last); print(" "); print(diffs[i]);print(" ");
        }
        last = diffs[i];
//      }
    } else {
      last = diffs[i];
    }
    
  }
//  println(diffs);
//  println(changes);
  
  if (changes >= 2) {
    return true;
  } else {
    return false;
  } 
//  print("lavg: ");
//  print(lavg);
//  print(" favg: ");
//  print(favg);
//  print(" or: ");
}
  

void serialEvent(Serial p) {
  try {
    inString = p.readStringUntil('\n');
    if (inString!=null) {
      inString = trim(inString);
      String[] values = splitTokens(inString);
      if (values.length == 4) {
        try {
          float x = float(values[0]);
          float y = float(values[1]);
          float z = float(values[2]);
          int m = int(values[3]);
          pushQueue(z);
          pushMagneticQueue(m);
          
          // calculate avg over the last 20 measures
          float avg = 0;
          for (int i=1; i<=20; i++) {
            avg = avg + rawQueue[rawQueue.length-i];
          }
          avg = avg / 20;
          pushAvgQueue(avg);
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


void pushQueue(float v) {
  float[] newQueue = subset(rawQueue, 1);
  float[] newQueue2 = append(newQueue, v);
  rawQueue = newQueue2;
}
void pushAvgQueue(float v) {
  float[] newQueue = subset(avgQueue, 1);
  float[] newQueue2 = append(newQueue, v);
  avgQueue = newQueue2;
}
void pushMagneticQueue(int v) {
  int[] newQueue = subset(magneticQueue, 1);
  int[] newQueue2 = append(newQueue, v);
  magneticQueue = newQueue2;
}
