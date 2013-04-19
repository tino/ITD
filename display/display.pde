import processing.serial.*;

Serial myPort;

boolean _running = true;
int FPS = 25;
String machines = "UX";
int[] states = new int[2];
int WIDTH = 1100;
int HEIGHT = 700;
int SHOW_BALANCE_DURATION = 4000;
int SHOW_UPDATE_DURATION = 5000;

long[] balanceTimes = new long[2];
int[] balances = new int[2];
int updateTime = -1 * SHOW_UPDATE_DURATION;

void setup() {
  frameRate(FPS);
  size(WIDTH, HEIGHT);
  println(Serial.list());
  String portName = Serial.list()[0];
  myPort = new Serial(this, portName, 57600);
  delay(1000);
  myPort.clear();
  delay(1000);
  updateScreen("A", "start", 1);
}

float toYScreen(float y) {
  return y;
}
float toXScreen(char machine, int x) {
  return (machines.indexOf(machine) * (WIDTH/2)) + x;
}

void draw() {
   background(255);
   
   // draw stuff for the machines
   for (int i=0; i<machines.length(); i++) {
     // boxes
     fill(255-(machines.charAt(i)-65));
     rect(toXScreen(machines.charAt(i), 10), 10, WIDTH/2-20, HEIGHT-20);
     
     // states
     textSize(16);
     fill(0);
     text(states[i], toXScreen(machines.charAt(i), 15), toYScreen(25));
     
     // show balances
     textSize(48);
     fill(105, 175, 244);
     if (millis() - balanceTimes[i] < SHOW_BALANCE_DURATION) {
       text("Your balance is: " + balances[i], toXScreen(machines.charAt(i), 20), 200);
     }
     
   }
   // show update
   textSize(60);
   fill(157, 26, 39);
   if (millis() - updateTime < SHOW_UPDATE_DURATION) {
     text("Balances updated!", 200, 500);
   }
   
   // show window
   textSize(40);
   fill(0);
   if (states[0] == 4 && states[1] == 4) {
     text("Update window open", 400, 600);
   }
}

void updateScreen(String sender, String action, int value) {

   if (action == "entrance") {
     
   } else if (action.equals("statechange")) {
     states[machines.indexOf(sender)] = value;
   } else if (action.equals("showbalance")) {
     balanceTimes[machines.indexOf(sender)] = millis();
     balances[machines.indexOf(sender)] = value;
   } else if (action.equals("updatebalance")) {
     updateTime = millis();
     balanceTimes[machines.indexOf(sender)] = millis();
     balances[machines.indexOf(sender)] = value;
   } else {
     print("ERROR unknown command: ");
     println(action);
   }
  
}


void serialEvent(Serial p) {
  try {
    String str = p.readStringUntil('\n');
    if (str!=null) {
      println(str);
      String[] m = match(str, "<<(.*?)>>");
      if (m[1].length() > 0) {
        String values[] = splitTokens(m[1], ":");
  
        if (values.length == 3 && values[0].length() == 1) {
          try {
            String sender = values[0];
            String action = values[1];
            int value = int(values[2]);
            updateScreen(sender, action, value);
          } 
          catch (Exception e) {
            println("Error converting values");
          }
        }
      }
    }
  } 
  catch(Exception e) {
    println("Error reading serial");
  }
}

void keyPressed() {
  if (_running) {
    _running = false;
  } 
  else {
    _running = true;
  }
}

