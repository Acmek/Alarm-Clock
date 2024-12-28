#include <TimeLib.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <EEPROM.h>

#include "Audio.h"
audio_t* sounds[] = { &GoodMorningKevin };
audio_t* helloSounds[] = { &HelloKevin, &ISeeYou };
audio_t* byeSounds[] = { &ByeKevin };

// Time
time_t RTCTime;

// Sound
byte wavePins[5] = {19, 20, 21, 22, 23};
audio_t* wave = sounds[0];
unsigned long wavePeriod = 125; // microseconds
unsigned long waveTimer = 0;
uint32_t currentWave = 0;
bool playSound = false;

// Audio Amplifier
// Total Potentiometer Resistance = 10147.5
// 3.5 + 1 Gain = 2899.285714
byte BOOST = 14;
unsigned long boostPeriod = 5; // half period (50% duty cycle) microseconds
unsigned long boostTimer = 0;
bool isHigh = false;

// Screen
byte dataPort[8] = {0, 1, 2, 3, 4, 5, 6, 7};
byte RS = 8;
byte RW = 9;
byte E = 10;

struct sendStruct {
  uint8_t isData; // 1 = sending data, 0 = sendind command
  byte data;
};
typedef struct sendStruct send_t;

bool sendingData = false;
send_t dataQueue[32];
uint8_t maxData = 0;
uint8_t dataHead = 0;
uint8_t dataTail = 0;
uint8_t sendingStage = 0;
unsigned long sendingTimer = 0;
uint8_t dataQueueSize = 0;

char alarmStr[8] = {'A', 'L', 'A', 'R', 'M', ' ', ' ', ' '};
char randomStr[8] = {'R', 'A', 'N', 'D', 'O', 'M', ' ', ' '};
char overheatStr[8] = {'O', 'V', 'E', 'R', 'H', 'E', 'A', 'T'};
char shortStr[8] = {'S', 'H', 'O', 'R', 'T', ' ', ' ', ' '};

// Buttons
byte buttonPins[3] = {11, 12, 13}; // up, down, enter
bool lastButtonState[3];
unsigned long debounceTime = 10000;
bool debounceActive[3];
unsigned long debounceTimer[3];
bool buttonPressed[3];
unsigned long holdTime = 500;
bool holdingActive[3];
unsigned long holdingTimer[3];
bool buttonHolding[3];

// Distance
byte TRIGGER = 15;
byte ECHO = 15;
bool sentPulse = false;
uint8_t distanceStage = 0;
unsigned long distanceTimer = 0;
unsigned long distanceCheckTime = 250000; // micros
uint8_t menuDistance = 100;
bool detected = false;
bool lastDetected = false;

// Menu
typedef void (*Function)();
struct menuStruct {
  char name[8];
  Function function;
  int32_t next[8];
};
typedef struct menuStruct menu_t;

void chooseMenu(void);
void autoNext(void);
void setDisplay(void);
void setStandard(void);
void setClockTime(void);
void setDate(void);
void setAlarmTime(void);
void setAlarmSound(void);
void setAlarmState(void);
void setMaxDistance(void);

menu_t menu[] = {
  {{'r', 'o', 'o', 't'}, chooseMenu, { 11, 9, 10, 1, 7, 8, 14, -1 }}, // 0
  {{'D', 'i', 's', 'p', 'l', 'a', 'y', ' '}, setDisplay, { 2, 3, 6, -1 }}, // 1
    {{'H', 'H', ':', 'M', 'M', ':', 'S', 'S'}, setStandard, { 4, 5, -1 }}, // 2
    {{'H', 'H', ':', 'M', 'M', ' ', 'A', 'M'}, setStandard, { 4, 5, -1 }}, // 3
      {{'S', 'T', 'A', 'N', 'D', 'A', 'R', 'D'}, autoNext, { 0, -1 }}, // 4
      {{'M', 'I', 'L', 'I', 'T', 'A', 'R', 'Y'}, autoNext, { 0, -1 }}, // 5
    {{'M', 'M', '/', 'D', 'D', '/', 'Y', 'Y'}, autoNext, { 0, -1 }}, // 6
  {{'S', 'e', 't', ' ', 'T', 'i', 'm', 'e'}, setClockTime, { 0, -1 }}, // 7
  {{'S', 'e', 't', ' ', 'D', 'a', 't', 'e'}, setDate, { 0, -1 }}, // 8
  {{'A', 'l', 'r', 'm', ' ', 'T', 'm', 'e'}, setAlarmTime, { 0, -1 }}, // 9
  {{'A', 'l', 'r', 'm', ' ', 'S', 'n', 'd'}, setAlarmSound, { 0, -1 }}, // 10
  {{'S', 'e', 't', ' ', 'A', 'l', 'r', 'm'}, setAlarmState, { 12, 13, -1 }}, // 11
    {{'O', 'N', ' ', ' ', ' ', ' ', ' ', ' '}, autoNext, { 0, -1 }}, // 12
    {{'O', 'F', 'F', ' ', ' ', ' ', ' ', ' '}, autoNext, { 0, -1 }}, // 13
  {{'S', 'e', 't', ' ', 'D', 'i', 's', 't'}, setMaxDistance, { 0, -1 }}, // 14
};
bool menuActive = false;
uint16_t currentMenu = 0;
uint16_t nextMenu = 0;

// Settings
uint8_t display = 1;
bool isStandard = true;
int alarmHour = 0;
int alarmMinute = 0;
int alarmSecond = 0;
bool alarmOn = true;
uint8_t maxDistance = 100; // centimeters
int soundIndex = 0;

// Menu Values
bool gotTime = false;
uint8_t settingIndex = 0;
int timeHour = 0;
int timeMinute = 0;
int timeSecond = 0;
int timeMonth = 0;
int timeDay = 0;
int timeYear = 0;
int menuAlarmHour = 0;
int menuAlarmMinute = 0;
int menuAlarmSecond = 0;

unsigned long blinkTime = 2000; // for settings indexes, millis
unsigned long blinkTimer = 0;

// Alarm
byte ALARMLED = 17;
bool activeAlarm = false;
unsigned long alarmBlinkTime = 2000; // millis
unsigned long alarmBlinkTimer = 0;

// Thermistor
byte THERMISTOR = 16;
// B = 3950, R3 = 9614.967
// T1 = 294.817K (71F), R1 = 8472.157
// T2 = 318.150K (45C), R2 = 3171.392
// B = ln(R1/R2) / (1/T1 - 1/T2)
// Bits = R2 / (R3 + R2) * (2^10 - 1) = 254
uint16_t thermistorBits = 254;
uint16_t lastBits = 0;
uint8_t shortBoundary = 150;
bool overheat = false;
bool shortCircuit = false;
unsigned long thermistorTime = 1000;
unsigned long thermistorTimer = 0;
unsigned long warningBlinkTime = 750; // millis
unsigned long warningBlinkTimer = 0;

void setup() {
  // put your setup code here, to run once:
  // initialize boost converter, write low in order to prevent floating gate signal on start up
  pinMode(BOOST, OUTPUT);
  digitalWrite(BOOST, LOW);
  
  // initialize other components
  InitLCD();
  writeString("LOADING");
  maxData = sizeof(dataQueue) / sizeof(send_t);

  pinMode(ALARMLED, OUTPUT);

  pinMode(THERMISTOR, INPUT);
  lastBits = analogRead(THERMISTOR) - 217;
  
  for (uint8_t i = 0; i < sizeof(wavePins) / sizeof(byte); i++) {
    pinMode(wavePins[i], OUTPUT);
  }

  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(byte); i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // loading saved values
  int seed = readEEPROM(0, 4, 0);
  randomSeed(seed);
  seed++;
  writeEEPROM(0, 4, seed);

  display = readEEPROM(4, 1, 1);
  if (readEEPROM(5, 1, 1) == 1) {
    isStandard = true;
  }
  else {
    isStandard = false;
  }
  alarmHour = readEEPROM(6, 4, 0);
  alarmMinute = readEEPROM(10, 4, 0);
  alarmSecond = readEEPROM(14, 4, 0);
  if (readEEPROM(18, 1, 1) == 1) {
    alarmOn = true;
  }
  else {
    alarmOn = false;
  }
  maxDistance = readEEPROM(19, 1, 100); // centimeters
  menuDistance = maxDistance;
  soundIndex = readEEPROM(20, 4, -1);
  setAudio();

  // initialize clock
  setSyncProvider(getTeensy3Time);
  timeStatus_t status = timeStatus();
  if (status == timeNotSet) {
    writeString("SET ERR");
    Teensy3Clock.set(1357041600);
  }
  else if (status == timeNeedsSync) {
    writeString("SYNC ERR");
  }
  setTime(getTeensy3Time());
}

void loop() {
  // put your main code here, to run repeatedly:
  //unsigned long startLoop = micros();
  //Serial.print("start ");
  //Serial.println(micros() - startLoop);

  checkThermistor();
  //Serial.print("analog ");
  //Serial.println(micros() - startLoop);
  
  checkSending();
  //Serial.print("sending ");
  //Serial.println(micros() - startLoop);

  if (!overheat && !shortCircuit) {
    checkBoost();
  }
  //Serial.print("boost ");
  //Serial.println(micros() - startLoop);

  checkWave();
  //Serial.print("wave ");
  //Serial.println(micros() - startLoop);

  checkDistance();
  //Serial.print("distance ");
  //Serial.println(micros() - startLoop);

  // if time initialization worked
  if (timeStatus() == timeSet) {
    readButtons();

    // get current time
    int currentHour = hour();
    int currentMinute = minute();
    int currentSecond = second();

    if (!activeAlarm && alarmOn) { // if alarm is on and NOT active
      if (currentHour == alarmHour && currentMinute == alarmMinute && currentSecond == alarmSecond) { // if the current time is the set alarm time
        // initialize audio and alarm
        setAudio();
        currentWave = 0;
        activeAlarm = true;
      }
    }

    if (activeAlarm) { // force alarm audio to continuously play
      playSound = true;
    }
    else { // distance sensor
      if (!menuActive) { // menu not opened
        if (detected && !lastDetected) { // appeared
          wave = helloSounds[random(0, sizeof(helloSounds) / sizeof(audio_t*))];
          currentWave = 0;
          playSound = true;
        }
        else if (!detected && lastDetected) { // disappeared
          wave = byeSounds[random(0, sizeof(byeSounds) / sizeof(audio_t*))];
          currentWave = 0;
          playSound = true;
        }
      }
    }
    lastDetected = detected;

    if (menuActive && !activeAlarm) { // call menu function if menu opened
      menu[currentMenu].function();
    }
    else {
      // alarm takes priority over safety
      if (activeAlarm && millis() - alarmBlinkTimer < alarmBlinkTime / 2) { // if alarm active
        // blink LED on and write 'ALARM'
        digitalWrite(ALARMLED, HIGH);
        sendString(alarmStr);
      }
      else if (!activeAlarm && (overheat || shortCircuit) && millis() - warningBlinkTimer < warningBlinkTime / 2) { // if alarm not active and thermistor detected overheat or short
        // blink LED on and write warning string
        digitalWrite(ALARMLED, HIGH);
        if (overheat) { sendString(overheatStr); }
        else if (shortCircuit) { sendString(shortStr); }
      }
      else { // alarm or led not blinking
        // blink LED off
        if (activeAlarm) {
          digitalWrite(ALARMLED, LOW);
        }
        else if (overheat || shortCircuit) {
          digitalWrite(ALARMLED, LOW);
        }

        // display time
        if (display == 0 || display == 1) { // HH:MM:SS or HH:MM AM
          // get current hours and convert to standard if set
          int hours = currentHour;
          if (isStandard) {
            hours = currentHour % 12;
            if (hours == 0) {
              hours = 12;
            }
          }

          // initialize time to string function
          int timeArr[3] = { hours, currentMinute, currentSecond };
          char timeStr[8] = {};
          timeToStr(timeArr, timeStr, ':');

          // if HH:MM AM, replace seconds with AM or PM
          if (display == 1) {
            timeStr[5] = ' ';
            timeStr[7] = 'M';
            if (hour() < 12) {
              timeStr[6] = 'A';
            }
            else {
              timeStr[6] = 'P';
            }
          }

          // write string
          sendString(timeStr);
        }
        else { // write date
          int timeArr[3] = { month(), day(), year() % 100 };
          char timeStr[8] = {};
          timeToStr(timeArr, timeStr, '/');
          sendString(timeStr);
        }
      }
    }
    // reset alarm or warning blink timers
    if (activeAlarm && millis() - alarmBlinkTimer >= alarmBlinkTime) {
      alarmBlinkTimer = millis();
    }
    if ((overheat || shortCircuit) && millis() - warningBlinkTimer >= warningBlinkTime) {
      warningBlinkTimer = millis();
    }
    //Serial.print("time ");
    //Serial.println(micros() - startLoop);

    // allow menu interface if no alarm or warning
    if (!activeAlarm && !overheat && !shortCircuit) {
      if (!menuActive && buttonPressed[2]) { // enter menu
        menuActive = true;
      }
      if (menuActive && buttonHolding[2]) { // exit and reset values
        currentMenu = 0;
        gotTime = false;
        settingIndex = 0;
        menuDistance = maxDistance;
        soundIndex = readEEPROM(20, 4, -1);
        menuActive = false;
      }
    }
    else { // if alarm is active
      if (activeAlarm && buttonPressed[2]) { // press ENTER to cancel alarm
        digitalWrite(ALARMLED, LOW);
        playSound = false;
        activeAlarm = false;
      }
    }

    resetButtons();
  }

  //Serial.print("done ");
  //Serial.println(micros() - startLoop);
}

void checkThermistor() {
  // ADC takes a long time to read, so add timer to periodically check
  // stop checking if overheat, as there is no way to reset clock unless you cut power
  if (!overheat && !shortCircuit && millis() - thermistorTimer >= thermistorTime) {
    int16_t temperatureBits = analogRead(THERMISTOR) - 217; // voltage drop from diode
    // overheat
    if (temperatureBits <= thermistorBits) {
      //Serial.print(temperatureBits);
      //Serial.println(" overheat");
      digitalWrite(BOOST, LOW);
      overheat = true;
    }
    /*else {
      Serial.print(temperatureBits);
      Serial.println(" good");
    }*/
    // short circuit because jump in "temperature" or the 3.3V rail was too large
    if (temperatureBits - lastBits >= shortBoundary) {
      digitalWrite(BOOST, LOW);
      shortCircuit = true;
    }

    lastBits = temperatureBits;
    thermistorTimer = millis();
  }
}

void checkDistance() {
  //Serial.println(digitalRead(ECHO));
  if (sentPulse) {
    if (distanceStage == 0) { // trigger high
      //Serial.println("stage 0");
      if (micros() - distanceTimer >= 2) {
        //Serial.println("stage 0 done");
        digitalWrite(TRIGGER, HIGH);
        distanceStage = 1;
        distanceTimer = micros();
      }
    }
    else if (distanceStage == 1) { // trigger low and pulse will be automatically sent
      //Serial.println("stage 1");
      if (micros() - distanceTimer >= 10) {
        //Serial.println("stage 1 done");
        digitalWrite(TRIGGER, LOW);
        pinMode(ECHO, INPUT); // set same pin to input to detect when sensor changes input
        distanceStage = 2;
        distanceTimer = micros();
      }
    }
    else if (distanceStage == 2) { // wait for echo pin to initialize
      //Serial.print("stage 2 ");
      //Serial.println(digitalRead(ECHO));
      if (micros() - distanceTimer >= 10000) { // 10 ms timeout
        distanceStage = 4;
        sentPulse = false;
        distanceTimer = micros();
      }
      else if (digitalRead(ECHO)) { // has initialized
        //Serial.println("stage 2 done");
        distanceStage = 3;
        distanceTimer = micros();
      }
    }
    else if (distanceStage == 3) { // wait for pulse back
      //Serial.println("stage 3");
      if (micros() - distanceTimer >= maxDistance * 58.82) { // maximum distance reached, send another pulse
        //Serial.println("not detected");
        detected = false;
        distanceStage = 4;
        sentPulse = false;
        distanceTimer = micros();
      }
      else if (!digitalRead(ECHO)) { // object detected
        //Serial.println("detected");
        detected = true;
        distanceStage = 4;
        sentPulse = false;
        distanceTimer = micros();
      }
    }
  }
  else { // periodically send pulse
    if (micros() - distanceTimer >= distanceCheckTime) {
      //Serial.println("sent");
      pinMode(TRIGGER, OUTPUT);
      digitalWrite(TRIGGER, LOW);
      distanceStage = 0;
      sentPulse = true;
      distanceTimer = micros();
    }
  }
}

void checkSending() {
  // the standard write for the display was converted to a queue system, as the original functions used delay
  // these functions use a separate timer, allowing other functions to run
  if (sendingStage == 0) {
    if (dataQueueSize > 0) {
      send_t data = dataQueue[dataTail];

      for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(dataPort[i], bitRead(data.data, i));
      }
      digitalWrite(RS, data.isData);      //RS set to HIGH for data
      digitalWrite(RW, LOW);      //R/W set to LOW  for writing
      digitalWrite(E, HIGH);     //E set to HIGH for latching

      sendingStage = 1;
      sendingTimer = micros();
    }
  }
  else if (sendingStage == 1) {
    if (micros() - sendingTimer >= 1000) {
      digitalWrite(E, LOW);
      sendingStage = 0;
      if (++dataTail == maxData) { dataTail = 0; }
      dataQueueSize -= 1;
    }
  }
}

void addSending(send_t data) { // add to queue for function above
  if (dataQueueSize < maxData) {
    dataQueue[dataHead].isData = data.isData;
    dataQueue[dataHead].data = data.data;

    if (++dataHead == maxData) { dataHead = 0; }
    dataQueueSize += 1;
  }
}

void checkBoost() {
  if (micros() - boostTimer > boostPeriod) { // write switching frequency
    if (isHigh) {
      digitalWrite(BOOST, LOW);
      isHigh = false;
    }
    else {
      digitalWrite(BOOST, HIGH);
      isHigh = true;
    }

    boostTimer = micros();
  }
}

void checkWave() {
  //Serial.println(micros() - waveTimer);
  if (playSound && micros() - waveTimer >= wavePeriod) { // time to go to next sample in .WAV file
    uint8_t num = (*wave).audio[currentWave];
    if (overheat || shortCircuit) { num *= 0.4329; } // 4.5 gain adjust to 5v if overheat or shortcircuit, since boost converter is now off

    for (uint8_t i = 0; i < sizeof(wavePins) / sizeof(byte); i++) { // write to DAC
      digitalWrite(wavePins[i], (num & (0b01 << i)) >> i);
    }

    currentWave = currentWave + 1; // go to next wave element
    if (currentWave >= (*wave).size) { // reached end of wave, stop sound
      //delay(1000);
      playSound = false;
      currentWave = 0;
    }

    waveTimer = micros(); // reset timer
  }
}

uint32_t readEEPROM(uint32_t address, uint32_t size, uint32_t defaultValue) {
  // easy way to get data given the memory address, number of bytes, and default value (returns if 255 was found)
  bool notWritten = true;
  uint32_t value = 0;
  for (uint32_t i = 0; i < size; i++) {
    uint8_t byteValue = EEPROM.read(address + i);
    if (byteValue != 0xFF) {
      notWritten = false;
    }
    value |= byteValue << (8 * i);
  }

  if (notWritten) {
    return defaultValue;
  }
  return value;
}

void writeEEPROM(uint32_t address, uint32_t size, uint32_t value) { // yeah
  for (uint32_t i = 0; i < size; i++) {
    EEPROM.write(address + i, (value & (0xFF << (8 * i))) >> (8 * i));
  }
}

// menu functions
void chooseMenu(void) {
  incrementNext();
  displayNext();
  int prevNext = setNext();

  if (prevNext != -1) { // sets displayed settings as the ones saved previously
    switch (currentMenu) {
      case 1: nextMenu = display; break;
      case 11: if (!alarmOn) { nextMenu = 1; } break;
    }
  }
}
void autoNext(void) { // automatically go to the next menu
  currentMenu = menu[currentMenu].next[nextMenu];
  nextMenu = 0;
}
void setDisplay(void) { // set display mode
  incrementNext();
  displayNext();
  int prevNext = setNext();
  if (prevNext != -1) {
    writeEEPROM(4, 1, prevNext);
    display = prevNext;

    if (!isStandard) { nextMenu = 1; }
  }
}
void setStandard(void) { // set if display is in standard or military
  incrementNext();
  displayNext();
  int prevNext = setNext();
  if (prevNext != -1) {
    if (prevNext == 0) {
      writeEEPROM(5, 1, 1);
      isStandard = true;
    }
    else {
      writeEEPROM(5, 1, 0);
      isStandard = false;
    }
  }
}
// set clock, date, and alarm are very long functions and I am tired. I wil try my best to explain clock, and rest is the exact same
void setClockTime(void) {
  if (!gotTime) { // get these values when menu begins
    // activate cursor sendCommand();
    timeHour = hour();
    timeMinute = minute();
    timeSecond = second();
    gotTime = true;
    blinkTimer = millis();
  }

  // get the time string
  int timeArr[3] = { timeHour, timeMinute, timeSecond };
  char timeStr[8] = {};
  timeToStr(timeArr, timeStr, ':');

  // blink current index value
  if (millis() - blinkTimer >= blinkTime / 2) {
    uint8_t strIndex = settingIndex;
    if (strIndex > 1) { strIndex++; }
    if (strIndex > 4) { strIndex++; }
    timeStr[strIndex] = 0xFF;
  }
  if (millis() - blinkTimer >= blinkTime) {
    blinkTimer = millis();
  }

  // write the time string
  sendString(timeStr);

  if (buttonPressed[0] || buttonPressed[1]) { // if up or down pressed
    // edit time, setting limits based on which index your cursor is at
    int change = 1;
    if (buttonPressed[1]) {
      change = -1;
    }
    if (settingIndex == 0 || settingIndex == 2 || settingIndex == 4) {
      change *= 10;
    }

    switch (settingIndex) {
      case 0:
      case 1: timeHour += change; break;
      case 2:
      case 3: timeMinute += change; break;
      case 4:
      case 5: timeSecond += change; break;
    }

    if (timeHour < 0 || timeHour > 24) { timeHour -= change; }
    if (timeMinute < 0 || timeMinute > 59) { timeMinute -= change; }
    if (timeSecond < 0 || timeSecond > 59) { timeSecond -= change; }
  }

  if (buttonPressed[2]) { // enter
    // move to the next index
    settingIndex += 1;
    if (settingIndex == 6) { // has finished. set new value and exit menu
      settingIndex = 0;
      int prevNext = setNext();
      if (prevNext != -1) {
        setTime(timeHour, timeMinute, timeSecond, day(), month(), year());
        Teensy3Clock.set(now());

        gotTime = false;
      }
    }
  }
}
void setDate(void) {
  if (!gotTime) {
    // activate cursor sendCommand();
    timeMonth = month();
    timeDay = day();
    timeYear = year() % 100;
    gotTime = true;
    blinkTimer = millis();
  }

  int timeArr[3] = { timeMonth, timeDay, timeYear };
  char timeStr[8] = {};
  timeToStr(timeArr, timeStr, '/');
  if (millis() - blinkTimer >= blinkTime / 2) {
    uint8_t strIndex = settingIndex;
    if (strIndex > 1) { strIndex++; }
    if (strIndex > 4) { strIndex++; }
    timeStr[strIndex] = 0xFF;
  }
  if (millis() - blinkTimer >= blinkTime) {
    blinkTimer = millis();
  }
  sendString(timeStr);

  if (buttonPressed[0] || buttonPressed[1]) {
    int change = 1;
    if (buttonPressed[1]) {
      change = -1;
    }
    if (settingIndex == 0 || settingIndex == 2 || settingIndex == 4) {
      change *= 10;
    }

    switch (settingIndex) {
      case 0:
      case 1: timeMonth += change; break;
      case 2:
      case 3: timeDay += change; break;
      case 4:
      case 5: timeYear += change; break;
    }

    if (timeMonth < 1 || timeMonth > 12) { timeMonth -= change; }
    int maxDay = 31;
    switch (timeMonth) {
      case 4: 
      case 6:
      case 9:
      case 11: maxDay = 30; break;
      case 2: maxDay = 29; break;
    }
    if (timeDay < 1 || timeDay > maxDay) { timeDay -= change; }
    if (timeYear < 0 || timeYear > 99) { timeYear -= change; }
  }

  if (buttonPressed[2]) {
    settingIndex += 1;
    if (settingIndex == 6) {
      settingIndex = 0;
      int prevNext = setNext();
      if (prevNext != -1) {
        if (!(!(timeYear % 400 == 0 || (timeYear % 4 == 0 && timeYear % 100 != 0)) && timeDay == 29)) {
          setTime(hour(), minute(), second(), timeDay, timeMonth, ((year() / 100) * 100) + timeYear);
          Teensy3Clock.set(now());
        }

        gotTime = false;
      }
    }
  }
}
void setAlarmState(void) { // simple enough to understand
  incrementNext();
  displayNext();
  int prevNext = setNext();
  if (prevNext != -1) {
    if (prevNext == 0) {
      writeEEPROM(18, 1, 1);
      alarmOn = true;
    }
    else {
      writeEEPROM(18, 1, 0);
      alarmOn = false;
    }
  }
}
void setAlarmTime(void) {
  if (!gotTime) {
    // activate cursor sendCommand();
    menuAlarmHour = alarmHour;
    menuAlarmMinute = alarmMinute;
    menuAlarmSecond = alarmSecond;
    gotTime = true;
    blinkTimer = millis();
  }

  int timeArr[3] = { menuAlarmHour, menuAlarmMinute, menuAlarmSecond };
  char timeStr[8] = {};
  timeToStr(timeArr, timeStr, ':');
  if (millis() - blinkTimer >= blinkTime / 2) {
    uint8_t strIndex = settingIndex;
    if (strIndex > 1) { strIndex++; }
    if (strIndex > 4) { strIndex++; }
    timeStr[strIndex] = 0xFF;
  }
  if (millis() - blinkTimer >= blinkTime) {
    blinkTimer = millis();
  }
  sendString(timeStr);

  if (buttonPressed[0] || buttonPressed[1]) {
    int change = 1;
    if (buttonPressed[1]) {
      change = -1;
    }
    if (settingIndex == 0 || settingIndex == 2 || settingIndex == 4) {
      change *= 10;
    }

    switch (settingIndex) {
      case 0:
      case 1: menuAlarmHour += change; break;
      case 2:
      case 3: menuAlarmMinute += change; break;
      case 4:
      case 5: menuAlarmSecond += change; break;
    }

    if (menuAlarmHour < 0 || menuAlarmHour > 24) { menuAlarmHour -= change; }
    if (menuAlarmMinute < 0 || menuAlarmMinute > 59) { menuAlarmMinute -= change; }
    if (menuAlarmSecond < 0 || menuAlarmSecond > 59) { menuAlarmSecond -= change; }
  }

  if (buttonPressed[2]) {
    settingIndex += 1;
    if (settingIndex == 6) {
      settingIndex = 0;
      int prevNext = setNext();
      if (prevNext != -1) {
        writeEEPROM(6, 4, menuAlarmHour);
        writeEEPROM(10, 4, menuAlarmMinute);
        writeEEPROM(14, 4, menuAlarmSecond);
        alarmHour = menuAlarmHour;
        alarmMinute = menuAlarmMinute;
        alarmSecond = menuAlarmSecond;
        gotTime = false;
      }
    }
  }
}
void setAlarmSound(void) {
  if (buttonPressed[0]) { // decrement in the array and play new sound
    if (soundIndex > -1) {
      soundIndex -= 1;

      if (soundIndex != -1) {
        wave = sounds[soundIndex];
        currentWave = 0;
        playSound = true;
      }
      else {
        playSound = false;
      }
    }
  }

  if (buttonPressed[1]) { // increment in the array and play new sound
    if (soundIndex + 1 < sizeof(sounds) / sizeof(audio_t*)) {
      soundIndex += 1;

      if (soundIndex != -1) {
        wave = sounds[soundIndex];
        currentWave = 0;
        playSound = true;
      }
      else {
        playSound = false;
      }
    }
  }

  if (soundIndex == -1) { // random selected. just write RANDOM
    sendString(randomStr);
  }
  else { // write sound name
    sendString((*sounds[soundIndex]).name);
  }

  int prevNext = setNext();
  if (prevNext != -1) { // set sound name
    writeEEPROM(20, 4, soundIndex);
    setAudio();
  }
}
void setAudio() { // sets audio based on if random or has already been set
  if (soundIndex != -1) {
    wave = sounds[soundIndex];
  }
  else {
    wave = sounds[random(0, sizeof(sounds) / sizeof(audio_t*))];
  }
}
void setMaxDistance(void) { // sets maximum distance for the distance sensor
  // edit distance
  if (buttonPressed[0]) {
    if (menuDistance < 200) {
      menuDistance += 10;
    }
  }
  
  if (buttonPressed[1]) {
    if (menuDistance >= 10) {
      menuDistance -= 10;
    }
  }

  // convert distance to string
  char distStr[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  distToStr(menuDistance, distStr);
  sendString(distStr);

  if (buttonPressed[2]) { // save distance
    int prevNext = setNext();
    if (prevNext != -1) {
      writeEEPROM(19, 1, menuDistance);
      maxDistance = menuDistance;
    }
  }
}
void incrementNext(void) { // used to display different menu options if available
  if (buttonPressed[1]) {
    if (menu[currentMenu].next[nextMenu + 1] != -1) {
      nextMenu += 1;
    }
  }
  if (buttonPressed[0]) {
    if (nextMenu > 0) {
      nextMenu -= 1;
    }
  }
}
void displayNext(void) { // displays next menu name based on next selected menu
  sendString(menu[menu[currentMenu].next[nextMenu]].name);
}
int setNext(void) { // if enter, set current menu as new
  // hold button
  if (buttonPressed[2]) {
    int prevNext = nextMenu;
    currentMenu = menu[currentMenu].next[nextMenu];
    nextMenu = 0;
    return prevNext;
  }
  // returns -1 so most functions can just test for this value if someone has pressed enter
  return -1;
}

void timeToStr(int* nums, char* str, char separator) { // given an 3 size array of nums, convert them into 3 separate numbers to display with a given separator between them and store in str
  str[2] = separator;
  str[5] = separator;
  for (uint8_t i = 0; i < 3; i++) {
    int num = nums[i];

    str[i + (i * 2)] = char(num / 10 + 0x30);
    str[i + 1 + (i * 2)] = char(num % 10 + 0x30);
  }
}

void distToStr(int num, char* distStr) { // converts distance to string and starts in distStr
  // not possible bu tjust in case
  if (num < 0) {
    num *= -1;
  }

  // use stack as digits are calculate from right to left, but we write right to left
  // stack flips it
  char stack[8] = {};
  uint8_t top = 0;
  do {
    uint32_t digit = num % 10;
    if (top < 5) { // max is 5 in order to make room for the CM
      // basic number to string algo
      stack[top] = char(digit + 0x30);
      top++;
    }
    else { // stop because you have reached too many numbers
      break;
    }
    num /= 10;
  } while (num != 0);

  // write numbers
  for (uint8_t i = 0; i < top; i++) {
    distStr[i] = stack[top - i - 1];
  }
  // write
  distStr[top + 1] = 'C';
  distStr[top + 2] = 'M';
}

void resetButtons() {
  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(byte); i++) {
    buttonPressed[i] = false;
    buttonHolding[i] = false;
  }
}

void readButtons() {
  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(byte); i++) {
    uint8_t input = !digitalRead(buttonPins[i]);

    // debounces and also detects if player is holding button, but I do not care to explain
    if (!debounceActive[i]) {
      if (input != lastButtonState[i]) {
        debounceTimer[i] = micros();
        debounceActive[i] = true;
      }
      else if (input == lastButtonState[i] && input == 1) {
        if (holdingActive[i] && millis() - holdingTimer[i] >= holdTime) {
          buttonHolding[i] = true;
        }
      }
    }

    if (debounceActive[i] && micros() - debounceTimer[i] > debounceTime) {
      if (input != lastButtonState[i]) {
        if (input && !lastButtonState[i]) {
          holdingTimer[i] = millis();
          holdingActive[i] = true;
        }
        else if (!input && lastButtonState[i]) {
          if (millis() - holdingTimer[i] < holdTime) {
            buttonPressed[i] = true;
          }
          holdingActive[i] = false;
        }

        lastButtonState[i] = input;
      }

      debounceActive[i] = false;
    }
  }
}

void writeString(String str) { // method to write to screen using delays
  if (str.length() > 8) {
    writeString("WRT ERR ");
  }
  else {
    command(0x01);
    command(0x80);
    for(uint8_t i = 0; i < str.length(); i++) {
      data(char(str[i]));
    }
  }
}

void sendString(char* str) { // method to write to screen using timers
  if (dataQueueSize + 9 < maxData) {
    sendCommand(0x80);
    for(uint8_t i = 0; i < 8; i++) {
      sendData(str[i]);
    }
  }
}

void InitLCD() { // uses delays but doesnt matter
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(dataPort[i], OUTPUT);
  }
  pinMode(RS, OUTPUT);
  pinMode(RW, OUTPUT);
  pinMode(E, OUTPUT);

  digitalWrite(E, LOW);       //Set E  LOW
  delay(100);
  command(0x30);              //command 0x30 = Wake up
  delay(30); 
  command(0x30);              //command 0x30 = Wake up #2
  delay(10);
  command(0x30);              //command 0x30 = Wake up #3
  delay(10); 
  command(0x30);              //Function set: 8-bit/1-line/5x8-font
  command(0x0C);              //Display ON; Cursor/Blinking OFF
  command(0x10);              //Entry mode set
  command(0x06);
  command(0x80);
}

void sendCommand(byte c) { //Function that sends commands
  send_t data = { 0, c };
  addSending(data);
}

void sendData(byte d) { //Function that sends data
  send_t data = { 1, d };
  addSending(data);
}

void command(byte c) { //Function that sends commands
  for(uint8_t i = 0; i < 8; i++) {
    digitalWrite(dataPort[i], bitRead(c, i));
  }
   
  digitalWrite(RS, LOW);      //RS set to LOW for command
  digitalWrite(RW, LOW);      //R/W set to LOW  for writing
  digitalWrite(E, HIGH);     //E set to HIGH for latching
  delay(1);
  digitalWrite(E, LOW);       //E set to LOW for latching
}

void data(byte d) { //Function that sends data
  for(uint8_t i = 0; i < 8; i++) {
    digitalWrite(dataPort[i], bitRead(d, i));
  }
  
  digitalWrite(RS, HIGH);      //RS set to HIGH for data
  digitalWrite(RW, LOW);      //R/W set to LOW  for writing
  digitalWrite(E, HIGH);     //E set to HIGH for latching
  delay(1);
  digitalWrite(E, LOW);       //E set to LOW for latching
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}
