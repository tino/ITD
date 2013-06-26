#include "Timer.h"

void noOp() {}

TimerClass::TimerClass() {
  for (int i = 0; i < 5; i++) {
    callbacks[i] = noOp;
    times[i] = 0;
  }
}

void TimerClass::runAt(callbackFunction function, unsigned long time) {
  if (time < millis())
    return;

  for (int i=0; i < 5; i++) {
    if (callbacks[i] == noOp) {
      callbacks[i] = function;
      times[i] = time;
    }
  }
}

void TimerClass::run() {
  for (int i = 0; i < 5; i++) {
    if (times[i] > millis()) {
      (*callbacks[i])();
      callbacks[i] = noOp;
      times[i] = 0;
    }
  }
}
