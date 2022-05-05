//Written By Nikodem Bartnik - nikodembartnik.pl
//Yoinked By Jochem Hoorneman
#include <Ds1302.h>

// RTC pins
#define PIN_ENA 7
#define PIN_DAT 6
#define PIN_CLK 5

// stepper motor pints
#define STEPPER_PIN_1 9
#define STEPPER_PIN_2 10
#define STEPPER_PIN_3 11
#define STEPPER_PIN_4 12

// button pins
#define HAND_ADJUST_PIN 2
#define TWELVE_OCLOCK_PIN 3
#define ADD_HOUR_PIN 4

// debug options
//#define DEBUG
//#define INFO

// DS1302 RTC instance
Ds1302 rtc(PIN_ENA, PIN_CLK, PIN_DAT);
Ds1302::DateTime now;

// number of stepper motor steps per revolution
int NUM_STEPS_PER_REV = 2048;
// phase of a step. Used give the right instructions to the stepper motor 
int step_phase = 0;
// current step the motor is at. 0 should be the clock hand pointing upwards (00:00 on a normal clock)
int current_step = 0;
// goal of where the stepper motor should be, if this is not the same as current_step the motor will move
int goal_step = 0;
int last_goal_step = 0;

// keeps track of how long is takes for the main loop to finish to finetune the delay
unsigned long last_arduino_time;
// keeps track of the milliseconds after it is 00:00:00
unsigned long noon_in_millis;
// decides if the milli seconds should be tracked by the arduino
bool do_track_millis = false;


// state of buttons
int handAdjustState;
int twelveOClockState;
int lastTwelveOClockState;
int addHourState;
int lastAddHourState;


void setup() {
  Serial.begin(9600);
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);
  pinMode(HAND_ADJUST_PIN, INPUT);
  pinMode(TWELVE_OCLOCK_PIN, INPUT);
  pinMode(ADD_HOUR_PIN, INPUT);

  rtc.init();
  // test if clock is halted and set a date-time (see example 2) to start it
  if (rtc.isHalted())
  {
    #ifdef INFO
      Serial.println("Time is reset to twelve o clock");
    #endif  
    resetTimeToTwelveOClock();
  }
  
  
  last_arduino_time = micros();
}

void loop() {
  // read button input
  handAdjustState = digitalRead(HAND_ADJUST_PIN);
  twelveOClockState = digitalRead(TWELVE_OCLOCK_PIN);
  addHourState = digitalRead(ADD_HOUR_PIN);
  #ifdef DEBUG
//    Serial.println("hand adjust, twelve o clock, add one hour");
    Serial.println(String(handAdjustState) + ", " + String(twelveOClockState) + ", " + String(addHourState));
  #endif

  // get and process current time
  rtc.getDateTime(&now);
  #ifdef INFO
    printCurrentTime();
  #endif
  
  // when it is close to noon, keep track of the milliseconds so the time is more accurate
  if(!do_track_millis && now.hour%12 == 0 && now.minute == 0 && now.second == 0) {
    noon_in_millis = millis();
    do_track_millis = true;
    #ifdef INFO
      Serial.println("Milliseconds get tracked now");
    #endif
  }

  // check if the time in milliseconds should still be tracked
  if(do_track_millis && millis() - noon_in_millis > 15000) {
    do_track_millis = false;
    #ifdef INFO
      Serial.println("Milliseconds are no longer tracked");
    #endif
  }

//  adjust the time so that the clock moves to the correct position
  if(do_track_millis) {
    float detailed_seconds = float(millis() - noon_in_millis)/1000;
    lnTimeToGoalStep(now.hour, now.minute, detailed_seconds);
  } else {
    lnTimeToGoalStep(now.hour, now.minute, now.second);
  }
    
  
  // processs button input
  if (lastAddHourState != addHourState && addHourState == HIGH) {
    setTimeTo((now.hour+1)%24, now.minute, now.second);
    #ifdef INFO
      Serial.println("Added one hour");
    #endif
  }
  if (lastTwelveOClockState != twelveOClockState && twelveOClockState == HIGH) {
    resetTimeToTwelveOClock();
    current_step = 0;
    goal_step = 0;
    #ifdef INFO
      Serial.println("Clock is reset to twelve o clock");
    #endif
  }

  int time_spend_in_loop = micros() - last_arduino_time;
//  Serial.println(time_spend_in_loop);
  // If the delay is set to 2000 microseconds. With 2048 steps per revolution this means 
  //  the clock can make a full revolution in 4.1 seconds
//  delayMicroseconds(max(10,2000-time_spend_in_loop));
  delayMicroseconds(2500);
  last_arduino_time = micros();
  
  // move motor if needed
  if (current_step != goal_step) {
    stepTowardsGoalStep();
  }
  else if (handAdjustState == HIGH) {
    oneStep(true);
  }

  // update last button states
  lastAddHourState = addHourState;
  lastTwelveOClockState = twelveOClockState;
  
}

void stepTowardsGoalStep(){
      oneStep(true);
      increaseCurrentStep();
  
//      int steps_to_take = modulus(goal_step - current_step, NUM_STEPS_PER_REV);
//    if (steps_to_take < NUM_STEPS_PER_REV/2) {
////      Serial.println(String(steps_to_take) + ", one step clockwise");
//      oneStep(true);
//      increaseCurrentStep();
//    }
//    else {
////      Serial.println(String(steps_to_take)+ ", one step anti-clockwise");
//      oneStep(false);
//      decreaseCurrentStep();
//    }
}

void resetTimeToTwelveOClock(){
  setTimeTo(12,0,0);
}

void setTimeTo(uint8_t hours, uint8_t minutes, uint8_t seconds) {
  Ds1302::DateTime dt = {
      .year = 0,
      .month = Ds1302::MONTH_JAN,
      .day = 1,
      .hour = hours,
      .minute = minutes,
      .second = seconds,
      .dow = Ds1302::DOW_MON
  };

  // set the date and time
  rtc.setDateTime(&dt);
}

void increaseCurrentStep() {
  increaseCurrentStep(1);
}

void decreaseCurrentStep() {
  increaseCurrentStep(-1);
}

void increaseCurrentStep(int stepsToAdd) {
  current_step = modulus(current_step + stepsToAdd, NUM_STEPS_PER_REV);
//  Serial.println("Current step updated, it is now: " + String(current_step));
}

void lnTimeToGoalStep(int hours, int minutes, float seconds) {
  lnTimeToGoalStep(hours+ (float) minutes/60 + seconds/3600);
}

void lnTimeToGoalStep(float timeInHours) {
  // converts time in hours to log time. For reference <lnTime = hh:mm:ss>
  // -9 = 00:00:00.27
  // -8 = 00:00:00.73
  // -7 = 00:00:01.99
  // -6 = 00:00:05.41
  // -5 = 00:00:14.71
  // -4 = 00:00:39.99
  // -3 = 00:01:48.71
  // -2 = 00:04:55.51
  // -1 = 00:13:23.27
  //  0 = 00:36:23.51
  //  1 = 01:38:55.40
  //  2 = 04:28:54.08
  if (fmod(timeInHours, 12) == 0) {
//    Serial.println("Skipped, since this would be -infinity");
  } else {
    int logTime = round(log(fmod(timeInHours, 12)));
    // move to the correct position on the clock. -9 logTime is located at the normal 12 o clock spot, -8 at the normal 1 spot etc
    timeToGoalStep(max(-9, logTime)+9);
  }
  

}

void timeToGoalStep(int hours, int minutes, int seconds) {
  timeToGoalStep(hours+ (float) minutes/60 + (float) seconds/3600);
}

void timeToGoalStep(float timeInHours) {
  timeInHours = fmod(timeInHours, 12);
  #ifdef DEBUG
    Serial.println("timeInHours after fmod: " + String(timeInHours));
  #endif
  goal_step = int(timeInHours * NUM_STEPS_PER_REV / 12);
  if (goal_step != last_goal_step) {
    #ifdef INFO
      Serial.println("Goal step changed from " + String(last_goal_step) + " to " + String(goal_step));
    #endif
  }
  last_goal_step = goal_step;
}

int modulus(int num, int denom) {
  // make sure it is above 0
  if (num < 0) {
    return (num - (num/denom-1)*denom);
  }
  else {
    return num%denom;
  }
}

void printCurrentTime() {
  static uint8_t last_second = 0;
    if (last_second != now.second)
    {
        last_second = now.second;

        Serial.print("20");
        Serial.print(now.year);    // 00-99
        Serial.print('-');
        if (now.month < 10) Serial.print('0');
        Serial.print(now.month);   // 01-12
        Serial.print('-');
        if (now.day < 10) Serial.print('0');
        Serial.print(now.day);     // 01-31
        Serial.print(' ');
        if (now.hour < 10) Serial.print('0');
        Serial.print(now.hour);    // 00-23
        Serial.print(':');
        if (now.minute < 10) Serial.print('0');
        Serial.print(now.minute);  // 00-59
        Serial.print(':');
        if (now.second < 10) Serial.print('0');
        Serial.print(now.second);  // 00-59
        Serial.println();
    }
}

void oneStep(bool doClockwise){
    if(doClockwise){
switch(step_phase){
  case 0:
  digitalWrite(STEPPER_PIN_1, HIGH);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
  break;
  case 1:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, HIGH);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
  break;
  case 2:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, HIGH);
  digitalWrite(STEPPER_PIN_4, LOW);
  break;
  case 3:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, HIGH);
  break;
} 
  }else{
    switch(step_phase){
  case 0:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, HIGH);
  break;
  case 1:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, HIGH);
  digitalWrite(STEPPER_PIN_4, LOW);
  break;
  case 2:
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, HIGH);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
  break;
  case 3:
  digitalWrite(STEPPER_PIN_1, HIGH);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
 
  
} 
  }
step_phase++;
  if(step_phase > 3){
    step_phase = 0;
  }
}
