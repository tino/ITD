/*
  Timer libarary
  For setting functions to execute at certain times
*/

#ifndef Timer_h
#define Timer_h

extern "C" {
  // callback function type
  typedef void (*callbackFunction)();
}

class TimerClass {
public:
  TimerClass();
  void runAt(callbackFunction function, unsigned long time);

private:
  callbackFunction callbacks[5];
  unsigned long runTimes[5];
};

extern TimerClass Timer;

#endif /* Timer_h */
