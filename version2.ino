
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// === ИКОНКИ ===
byte alarmIcon[8] = {B00100, B01110, B01110, B01110, B11111, B00000, B00100, B00000};
byte soundIcon[8] = {B00100, B01100, B11111, B11111, B11111, B01100, B00100, B00000};
byte muteIcon[8]  = {B10001, B01010, B00100, B01010, B10001, B00000, B00000, B00000};

// === РЕЖИМЫ ===
enum DisplayMode {
  MAIN_DISPLAY, ALARM_SET, STOPWATCH, GAME,
  SET_TIME, SET_VOLUME, CREDENTIALS_SCROLL
};
DisplayMode currentMode = MAIN_DISPLAY;

enum SettingsOption { ADJUST_TIME, ADJUST_VOLUME, CREDENTIALS, NUM_OPTIONS };
SettingsOption currentSetting = ADJUST_TIME;
bool inSettingsMenu = false;

// === ВРЕМЯ ===
unsigned long previousMillis = 0;
int currentSecond = 0, currentMinute = 0, currentHour = 0;

// === БУДИЛЬНИК ===
bool alarmSet = false;
int alarmHour = 0, alarmMinute = 0;
int selectedField = 0;
int melodyIndex = 0;
const int NUM_MELODIES = 3;
String melodies[NUM_MELODIES] = {"Tone1", "Tone2", "Tone3"};
bool soundOn = true;
bool alarmRinging = false;
bool alarmSnoozed = false;
unsigned long alarmStartTime = 0;
bool motorRunning = false;
int snoozeHour = 0, snoozeMinute = 0;

// === ПИНЫ ===
const int buzzerPin = 9;
const int modeButton = 4;
const int buttonUp = 5;
const int buttonDown = 6;
const int buttonRight = 7;
const int buttonSet = 8;
const int echoSensor = 2;
const int trigSensor = 3;
const int motorPin = A3;

bool lastToggleButtonState = LOW;
bool lastModeButtonState = LOW;

// === СТОПВАТЧ ===
bool stopwatchRunning = false;
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsed = 0;

// Звук
int volumeLevel = 5;

// === ДЛЯ ИГРЫ ===
#define SPRITE_RUN1 1
#define SPRITE_RUN2 2
#define SPRITE_JUMP 3
#define SPRITE_JUMP_UPPER '.'
#define SPRITE_JUMP_LOWER 4
#define SPRITE_TERRAIN_EMPTY ' '
#define SPRITE_TERRAIN_SOLID 5
#define SPRITE_TERRAIN_SOLID_RIGHT 6
#define SPRITE_TERRAIN_SOLID_LEFT 7
#define HERO_HORIZONTAL_POSITION 1
#define TERRAIN_WIDTH 16
#define TERRAIN_EMPTY 0
#define TERRAIN_LOWER_BLOCK 1
#define TERRAIN_UPPER_BLOCK 2
#define HERO_POSITION_OFF 0
#define HERO_POSITION_RUN_LOWER_1 1
#define HERO_POSITION_RUN_LOWER_2 2
#define HERO_POSITION_JUMP_1 3
#define HERO_POSITION_JUMP_2 4
#define HERO_POSITION_JUMP_3 5
#define HERO_POSITION_JUMP_4 6
#define HERO_POSITION_JUMP_5 7
#define HERO_POSITION_JUMP_6 8
#define HERO_POSITION_JUMP_7 9
#define HERO_POSITION_JUMP_8 10
#define HERO_POSITION_RUN_UPPER_1 11
#define HERO_POSITION_RUN_UPPER_2 12

char terrainUpper[TERRAIN_WIDTH + 1];
char terrainLower[TERRAIN_WIDTH + 1];
volatile bool buttonPushed = false;

// === НАСТРОЙКА ===
void setup() {
  lcd.init(); lcd.backlight();
  showLoadingAnimation();
  lcd.createChar(0, alarmIcon);
  lcd.createChar(1, soundIcon);
  lcd.createChar(2, muteIcon);
  pinMode(modeButton, INPUT);
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  pinMode(buttonRight, INPUT);
  pinMode(buttonSet, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(trigSensor, OUTPUT);
  pinMode(echoSensor, INPUT);
  pinMode(motorPin, OUTPUT);
  initializeGraphics();
}



// === ГЛАВНЫЙ LOOP ===
void loop() {
  updateTime();
  handleButtons();

  // === Настройки ===
  if (inSettingsMenu) {
    if (isButtonPressed(buttonUp)) {
      currentSetting = static_cast<SettingsOption>((currentSetting + 1) % NUM_OPTIONS);
      displayCurrentSetting();
      delay(200);
    }

    if (isButtonPressed(buttonDown)) {
      inSettingsMenu = false;
      switch (currentSetting) {
        case ADJUST_TIME:
          currentMode = SET_TIME;
          selectedField = 0;
          break;
        case ADJUST_VOLUME:
          currentMode = SET_VOLUME;
          break;
        case CREDENTIALS:
          currentMode = CREDENTIALS_SCROLL;
          break;
      }
      return;
    }

    if (isButtonPressed(buttonSet)) {
      inSettingsMenu = false;
      currentMode = MAIN_DISPLAY;
      lcd.clear();
      displayTime(currentHour, currentMinute);
      delay(200);
    }
    return;
  }

  // === Будильник ===
  if (alarmSet && !alarmRinging && currentHour == alarmHour && currentMinute == alarmMinute && currentSecond == 0) {
    alarmRinging = true;
    alarmStartTime = millis(); 
    playMelody(melodyIndex);
  }

  if (alarmSnoozed && !alarmRinging && currentHour == snoozeHour && currentMinute == snoozeMinute && currentSecond == 0) {
    alarmRinging = true;
    alarmSnoozed = false;
    playMelody(melodyIndex);
  }

  if (alarmRinging) {
  // If the alarm just started, note the time
  if (alarmStartTime == 0) {
    alarmStartTime = millis();
  }

  // Stop the alarm with Set button
  if (isButtonPressed(buttonSet)) {
    alarmRinging = false;
    noTone(buzzerPin);
    alarmSet = false;
    alarmStartTime = 0;
    motorRunning = false;
    digitalWrite(motorPin, LOW);
    delay(300);
  }
  // Snooze logic
  else if (
    isButtonPressed(modeButton) ||
    isButtonPressed(buttonUp) ||
    isButtonPressed(buttonDown) ||
    isButtonPressed(buttonRight)
  ) {
    alarmRinging = false;
    noTone(buzzerPin);
    alarmSnoozed = true;
    alarmStartTime = 0;
    motorRunning = false;
    digitalWrite(motorPin, LOW);
    snoozeMinute = currentMinute + 5;
    snoozeHour = currentHour;
    if (snoozeMinute >= 60) {
      snoozeMinute -= 60;
      snoozeHour = (snoozeHour + 1) % 24;
    }
    delay(300);
  }
  // Motor logic: start if 10+ sec passed, stop if object <10 cm
  else {
    unsigned long elapsed = millis() - alarmStartTime;
    if (elapsed >= 10000 && !motorRunning) {
      digitalWrite(motorPin, HIGH); // Start motor
      motorRunning = true;
    }

    if (motorRunning && getDistanceCM() <= 10) {
      digitalWrite(motorPin, LOW);  // Stop motor
      motorRunning = false;
    }
  }
}

  // === Вход в меню настроек ===
  if (currentMode == MAIN_DISPLAY && isButtonPressed(buttonSet)) {
    inSettingsMenu = true;
    displayCurrentSetting();
    delay(200);
    return;
  }

  // === Отображение текущего режима ===
  lcd.clear();
  switch (currentMode) {
    case MAIN_DISPLAY: showMainDisplay(); break;
    case ALARM_SET: showAlarmSetMode(); break;
    case STOPWATCH: showStopwatchMode(); break;
    case GAME: runDinoGame(); break;
    case SET_TIME: showSetTimeMode(); break;
    case SET_VOLUME: showSetVolumeMode(); break;
    case CREDENTIALS_SCROLL: showScrollingCredentials(); break;
  }

  // === Управление ALARM_SET ===
if (currentMode == ALARM_SET) {
  if (isButtonPressed(buttonRight)) selectedField = (selectedField + 1) % 3, delay(200);

  if (isButtonPressed(buttonUp)) {
    if (selectedField == 0) alarmHour = (alarmHour + 1) % 24;
    else if (selectedField == 1) alarmMinute = (alarmMinute + 1) % 60;
    else if (selectedField == 2) melodyIndex = (melodyIndex + 1) % NUM_MELODIES;
    delay(200);
  }

  if (isButtonPressed(buttonDown)) {
    if (selectedField == 0) alarmHour = (alarmHour - 1 + 24) % 24;
    else if (selectedField == 1) alarmMinute = (alarmMinute - 1 + 60) % 60;
    else if (selectedField == 2) melodyIndex = (melodyIndex - 1 + NUM_MELODIES) % NUM_MELODIES;
    delay(200);
  }

  if (isButtonPressed(buttonSet)) {
    alarmSet = true;
    currentMode = MAIN_DISPLAY;
    delay(200);
  }
}


  // === Управление SET_TIME ===
  if (currentMode == SET_TIME) {
    if (isButtonPressed(buttonRight)) selectedField = (selectedField + 1) % 2, delay(200);

    if (isButtonPressed(buttonUp)) {
      if (selectedField == 0) currentHour = (currentHour + 1) % 24;
      else currentMinute = (currentMinute + 1) % 60;
      delay(200);
    }

    if (isButtonPressed(buttonDown)) {
      if (selectedField == 0) currentHour = (currentHour - 1 + 24) % 24;
      else currentMinute = (currentMinute - 1 + 60) % 60;
      delay(200);
    }

    if (isButtonPressed(buttonSet)) {
      currentMode = MAIN_DISPLAY;
      delay(200);
    }
  }

  // === Управление SET_VOLUME ===
  if (currentMode == SET_VOLUME) {
    if (isButtonPressed(buttonUp) && volumeLevel < 10) volumeLevel++, delay(200);
    if (isButtonPressed(buttonDown) && volumeLevel > 0) volumeLevel--, delay(200);
    if (isButtonPressed(buttonSet)) currentMode = MAIN_DISPLAY, delay(200);
  }

  delay(200);
}

void showLoadingAnimation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Alarm Boot");

  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.print((char)255);  // █ блок
    delay(100);            // скорость загрузки
  }

  delay(500); // короткая пауза перед стартом
}


void playMelody(int index) {
  if (!soundOn || volumeLevel == 0) return;

  // Чем ниже громкость — тем короче тон и длиннее пауза
  int toneDuration = map(volumeLevel, 1, 10, 50, 300);   // громкость 1 → 50 мс, 10 → 300 мс
  int pauseDuration = map(volumeLevel, 1, 10, 250, 50);  // громкость 1 → 250 мс, 10 → 50 мс

  switch (index) {
    case 0: // Пик-пик
      for (int i = 0; i < 10; i++) {
        tone(buzzerPin, 1000);
        delay(toneDuration);
        noTone(buzzerPin);
        delay(pauseDuration);
      }
      break;

    case 1: {
      int melody[] = {440, 494, 523};
      for (int i = 0; i < 3; i++) {
        tone(buzzerPin, melody[i]);
        delay(toneDuration);
        noTone(buzzerPin);
        delay(pauseDuration);
      }
      break;
    }

    case 2: // Быстрые писки
      for (int i = 0; i < 20; i++) {
        tone(buzzerPin, 1500);
        delay(toneDuration / 2);   // Писк покороче
        noTone(buzzerPin);
        delay(pauseDuration / 2);
      }
      break;
  }
}

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===
bool isButtonPressed(int pin) { return digitalRead(pin) == HIGH; }

void handleButtons() {
  int toggleState = digitalRead(buttonSet);
  int modeState = digitalRead(modeButton);
  if (toggleState == HIGH && lastToggleButtonState == LOW) { soundOn = !soundOn; delay(200); }
  if (modeState == HIGH && lastModeButtonState == LOW) {
    currentMode = static_cast<DisplayMode>((currentMode + 1) % 4); delay(200);
  }
  lastToggleButtonState = toggleState;
  lastModeButtonState = modeState;
}

void updateTime() {
  if (millis() - previousMillis >= 1000) {
    previousMillis += 1000;
    currentSecond++;
    if (currentSecond >= 60) currentSecond = 0, currentMinute++;
    if (currentMinute >= 60) currentMinute = 0, currentHour = (currentHour + 1) % 24;
  }
}

void displayTime(int hour, int minute) {
  if (hour < 10) lcd.print("0"); lcd.print(hour); lcd.print(":");
  if (minute < 10) lcd.print("0"); lcd.print(minute);
}

void displayCurrentSetting() {
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Settings Menu");
  lcd.setCursor(0, 1);
  switch (currentSetting) {
    case ADJUST_TIME: lcd.print("> Adjust Time"); break;
    case ADJUST_VOLUME: lcd.print("> Adjust Volume"); break;
    case CREDENTIALS: lcd.print("> Credentials"); break;
  }
}

void showMainDisplay() {
  lcd.setCursor(0, 0); lcd.print("Time: "); displayTime(currentHour, currentMinute);
  lcd.setCursor(0, 1); lcd.write(byte(0));
  if (alarmSet) { lcd.print(" "); displayTime(alarmHour, alarmMinute); } else lcd.print(" Off ");
  lcd.print(" "); lcd.write(byte(soundOn ? 1 : 2));
}

void showAlarmSetMode() {
  lcd.setCursor(0, 0); lcd.print("Set Alarm:");
  lcd.setCursor(0, 1);
  lcd.print(selectedField == 0 ? ">" : " "); lcd.print(alarmHour < 10 ? "0" : ""); lcd.print(alarmHour); lcd.print(":");
  lcd.print(selectedField == 1 ? ">" : " "); lcd.print(alarmMinute < 10 ? "0" : ""); lcd.print(alarmMinute);
  if (selectedField == 2) lcd.setCursor(10, 1), lcd.print("*");
  lcd.setCursor(11, 1); lcd.print(melodies[melodyIndex]);
}

void showStopwatchMode() {
  unsigned long displayTimeMs = stopwatchRunning ? millis() - stopwatchStartTime : stopwatchElapsed;
  unsigned long totalSeconds = displayTimeMs / 1000;
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  lcd.setCursor(0, 0); lcd.print("Stopwatch Mode");
  lcd.setCursor(0, 1);
  if (minutes < 10) lcd.print("0"); lcd.print(minutes); lcd.print(":");
  if (seconds < 10) lcd.print("0"); lcd.print(seconds);
}



void showSetTimeMode() {
  lcd.setCursor(0, 0); lcd.print("Set Current Time:");
  lcd.setCursor(0, 1);
  lcd.print(selectedField == 0 ? ">" : " ");
  if (currentHour < 10) lcd.print("0"); lcd.print(currentHour); lcd.print(":");
  lcd.print(selectedField == 1 ? ">" : " ");
  if (currentMinute < 10) lcd.print("0"); lcd.print(currentMinute);
}

void showSetVolumeMode() {
  lcd.setCursor(0, 0); lcd.print("Set Volume:");
  lcd.setCursor(0, 1);
  for (int i = 0; i < volumeLevel; i++) lcd.print((char)255); // █ блоки громкости
}

void showScrollingCredentials() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Anuar   Diana");
  lcd.setCursor(0, 1);
  lcd.print("Pasha Nefor");
  delay(5000); // Показывать 3 секунды
  currentMode = MAIN_DISPLAY;
}

long getDistanceCM() {
  digitalWrite(trigSensor, LOW);
  delayMicroseconds(2);
  digitalWrite(trigSensor, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigSensor, LOW);
  long duration = pulseIn(echoSensor, HIGH);
  return duration * 0.034 / 2; // convert to cm
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------------------------------------------------
// GAME GAME GAME GAME GAME GAME GAME 


void showGameMode() {
  runDinoGame();
}
unsigned int maxScore = 0;

void runDinoGame() {
  // if (isButtonPressed(modeButton) && lastModeButtonState == LOW) {
  //   currentMode = static_cast<DisplayMode>((currentMode + 1) % 4);
  //   delay(300);
  //   lastModeButtonState = HIGH;
  //   return;
  // }

  if (currentMode != GAME) return;

  if (isButtonPressed(buttonUp)) {
    buttonPushed = true;
  }

  static byte heroPos = HERO_POSITION_RUN_LOWER_1;
  static byte newTerrainType = TERRAIN_EMPTY;
  static byte newTerrainDuration = 1;
  static bool playing = false;
  static bool blink = false;
  static unsigned int distance = 0;

  if (!playing) {
    drawHero(blink ? HERO_POSITION_OFF : heroPos, terrainUpper, terrainLower, 0);
    if (blink) {
      lcd.setCursor(0, 0); lcd.print("Press Start");
      lcd.setCursor(0, 1); lcd.print("Max: ");
      lcd.print(maxScore);
    }
    delay(250); blink = !blink;

    if (buttonPushed) {
      initializeGraphics();
      heroPos = HERO_POSITION_RUN_LOWER_1;
      playing = true;
      buttonPushed = false;
      distance = 0;
    }
    return;
  }

  advanceTerrain(terrainLower, newTerrainType == TERRAIN_LOWER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
  advanceTerrain(terrainUpper, newTerrainType == TERRAIN_UPPER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);

  if (--newTerrainDuration == 0) {
    newTerrainType = (newTerrainType == TERRAIN_EMPTY) ? ((random(3) == 0) ? TERRAIN_UPPER_BLOCK : TERRAIN_LOWER_BLOCK) : TERRAIN_EMPTY;
    newTerrainDuration = (newTerrainType == TERRAIN_EMPTY) ? 6 + random(6) : 1 + random(3);
  }

  if (buttonPushed) {
    if (heroPos <= HERO_POSITION_RUN_LOWER_2) heroPos = HERO_POSITION_JUMP_1;
    buttonPushed = false;
  }

  if (drawHero(heroPos, terrainUpper, terrainLower, distance >> 3)) {
    playing = false;
    if ((distance >> 3) > maxScore) maxScore = distance >> 3;
  } else {
    if (heroPos == HERO_POSITION_RUN_LOWER_2 || heroPos == HERO_POSITION_JUMP_8) heroPos = HERO_POSITION_RUN_LOWER_1;
    else if ((heroPos >= HERO_POSITION_JUMP_3 && heroPos <= HERO_POSITION_JUMP_5) && terrainLower[HERO_HORIZONTAL_POSITION] != SPRITE_TERRAIN_EMPTY) heroPos = HERO_POSITION_RUN_UPPER_1;
    else if (heroPos >= HERO_POSITION_RUN_UPPER_1 && terrainLower[HERO_HORIZONTAL_POSITION] == SPRITE_TERRAIN_EMPTY) heroPos = HERO_POSITION_JUMP_5;
    else if (heroPos == HERO_POSITION_RUN_UPPER_2) heroPos = HERO_POSITION_RUN_UPPER_1;
    else ++heroPos;
    ++distance;
  }
}

void initializeGraphics() {
  static byte graphics[] = {
    B11110, B10111, B11110, B11100, B11111, B11100, B10010, B11011,
    B11110, B10111, B11110, B11100, B11111, B11100, B01010, B10011,
    B11110, B10111, B11110, B11100, B11111, B11110, B10001, B00000,
    B11111, B11100, B10010, B11011, B00000, B00000, B00000, B00000,
    B00100, B11111, B00100, B00101, B11111, B00100, B00100, B00100,
    B00000, B00011, B00000, B00000, B00011, B00000, B00000, B00000,
    B00000, B11000, B00000, B01000, B11000, B00000, B00000, B00000,
  };
  for (int i = 0; i < 7; ++i) lcd.createChar(i + 1, &graphics[i * 8]);
  for (int i = 0; i < TERRAIN_WIDTH; ++i) terrainUpper[i] = SPRITE_TERRAIN_EMPTY, terrainLower[i] = SPRITE_TERRAIN_EMPTY;
}

void advanceTerrain(char* terrain, byte newTerrain) {
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    char current = terrain[i];
    char next = (i == TERRAIN_WIDTH - 1) ? newTerrain : terrain[i + 1];
    switch (current) {
      case SPRITE_TERRAIN_EMPTY: terrain[i] = (next == SPRITE_TERRAIN_SOLID) ? SPRITE_TERRAIN_SOLID_RIGHT : SPRITE_TERRAIN_EMPTY; break;
      case SPRITE_TERRAIN_SOLID: terrain[i] = (next == SPRITE_TERRAIN_EMPTY) ? SPRITE_TERRAIN_SOLID_LEFT : SPRITE_TERRAIN_SOLID; break;
      case SPRITE_TERRAIN_SOLID_RIGHT: terrain[i] = SPRITE_TERRAIN_SOLID; break;
      case SPRITE_TERRAIN_SOLID_LEFT: terrain[i] = SPRITE_TERRAIN_EMPTY; break;
    }
  }
}

bool drawHero(byte position, char* terrainUpper, char* terrainLower, unsigned int score) {
  bool collide = false;
  char upperSave = terrainUpper[HERO_HORIZONTAL_POSITION];
  char lowerSave = terrainLower[HERO_HORIZONTAL_POSITION];
  byte upper, lower;

  switch (position) {
    case HERO_POSITION_OFF: upper = lower = SPRITE_TERRAIN_EMPTY; break;
    case HERO_POSITION_RUN_LOWER_1: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN1; break;
    case HERO_POSITION_RUN_LOWER_2: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN2; break;
    case HERO_POSITION_JUMP_1: case HERO_POSITION_JUMP_8: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_JUMP; break;
    case HERO_POSITION_JUMP_2: case HERO_POSITION_JUMP_7: upper = SPRITE_JUMP_UPPER; lower = SPRITE_JUMP_LOWER; break;
    case HERO_POSITION_JUMP_3: case HERO_POSITION_JUMP_4: case HERO_POSITION_JUMP_5: case HERO_POSITION_JUMP_6: upper = SPRITE_JUMP; lower = SPRITE_TERRAIN_EMPTY; break;
    case HERO_POSITION_RUN_UPPER_1: upper = SPRITE_RUN1; lower = SPRITE_TERRAIN_EMPTY; break;
    case HERO_POSITION_RUN_UPPER_2: upper = SPRITE_RUN2; lower = SPRITE_TERRAIN_EMPTY; break;
  }

  if (upper != ' ') terrainUpper[HERO_HORIZONTAL_POSITION] = upper, collide = (upperSave != SPRITE_TERRAIN_EMPTY);
  if (lower != ' ') terrainLower[HERO_HORIZONTAL_POSITION] = lower, collide |= (lowerSave != SPRITE_TERRAIN_EMPTY);

  byte digits = (score > 9999) ? 5 : (score > 999) ? 4 : (score > 99) ? 3 : (score > 9) ? 2 : 1;
  char temp = terrainUpper[16 - digits];
  terrainUpper[16 - digits] = '\0';
  lcd.setCursor(0, 0); lcd.print(terrainUpper);
  terrainUpper[16 - digits] = temp;
  lcd.setCursor(0, 1); lcd.print(terrainLower);
  lcd.setCursor(16 - digits, 0); lcd.print(score);

  terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
  terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;
  return collide;
}
