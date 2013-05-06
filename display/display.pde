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
int[] lastBalanceChange = new int[2];
int updateTime = -1 * SHOW_UPDATE_DURATION;
int updateWindowOpen = 0;

color[] reds = new color[5];
color[] greens = new color[5];

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

  // setup colors
  reds[0] = color(255, 213, 213);
  reds[1] = color(255, 128, 128);
  reds[2] = color(255, 85, 85);
  reds[3] = color(255, 43, 43);
  reds[4] = color(255, 0, 0);

  greens[0] = color(211, 255, 204);
  greens[1] = color(167, 255, 154);
  greens[2] = color(124, 255, 102);
  greens[3] = color(80, 255, 51);
  greens[4] = color(0, 255, 0);

  lastBalanceChange[0] = 0;
  lastBalanceChange[1] = 1;

}

float toYScreen(float y) {
  return y;
}
float toXScreen(char machine, int x) {
  return (machines.indexOf(machine) * (WIDTH/2)) + x;
}

void draw() {
  background(255);

  // show update in fading circles
  if (millis() - updateTime < SHOW_UPDATE_DURATION) {
    noStroke();
     // draw stuff for the machines
    for (int i=0; i<machines.length(); i++) {
      float progress = millis() - updateTime;
      float alpha = abs(lastBalanceChange[i] - progress / SHOW_UPDATE_DURATION);
      fill(0, 0, 255, alpha*255);
      ellipse(toXScreen(machines.charAt(i), (WIDTH/2-20)/2), HEIGHT/2-10, 250, 250);
    }
  } else {
     // Balance is shown by the color of the box
    for (int i=0; i<machines.length(); i++) {
      if (millis() - balanceTimes[i] < SHOW_BALANCE_DURATION) {
        if (balances[i] > 0) {
          fill(greens[balances[i]-1]);
        } else if (balances[i] < 0) {
          fill(reds[-1*balances[i]-1]);
        } else {
          fill(255);
        }
      } else {
        fill(128); //  grey
      }
      // boxes
      rect(toXScreen(machines.charAt(i), 10), 10, WIDTH/2-20, HEIGHT-20);
    }
  }

  // states
  for (int i=0; i<machines.length(); i++) {
    textSize(16);
    fill(0);
    text(states[i], toXScreen(machines.charAt(i), 15), toYScreen(25));
  }


  // show window
  textSize(60);

  if (updateWindowOpen == 0) {
   if (states[0] == 4 && states[1] == 4) {
     updateWindowOpen = millis();
   }
  } else if (millis() - updateWindowOpen < 10000) {
   int secs = (11000 - (millis() - updateWindowOpen)) / 1000;
   fill(255);
   rect(WIDTH/2-70, HEIGHT/2-80, 140, 140);
   fill(0);
   text(secs, WIDTH/2-20, HEIGHT/2);
  } else {
   updateWindowOpen = 0;
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
     if (value > balances[machines.indexOf(sender)]) {
       lastBalanceChange[machines.indexOf(sender)] = 1;
     } else {
       lastBalanceChange[machines.indexOf(sender)] = 0;
     }
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

