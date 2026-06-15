#include <Arduino.h>
#include <FastLED.h>
#include <GyverOLED.h>
#include <EEPROM.h>
#include "config.h"

// ============================================================================
// 📦 ОБЪЕКТЫ И ПЕРЕМЕННЫЕ
// ============================================================================
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

Settings set;
CRGB leds[NUM_GLASSES];

enum MenuState {
  MAIN_SCREEN,
  FAST_DOSING,
  ROOT_MENU,
  SET_MODE,
  SUB_MENU_EDIT,
  SET_BASE_VOLUME,
  SET_INVITE_MENU,      // Настройки Зазывалы вместо рулетки
  SET_TOAST_TIME,
  SET_STALE_MENU,
  SET_STALE_TIME,
  CAL_PUMP,
  PUMP_FLUSH,
  RESET_PAGE,
  POURING,
  TOAST_SCREEN,
  SLEEP_MODE
};

enum WorkMode {
  MODE_MANUAL,
  MODE_AUTOMATIC
};

enum GlassStatus {
  EMPTY_SPACE,
  EMPTY_GLASS,
  POURING_GLASS,
  FILLED_GLASS,
  DISABLED_SPACE
};

MenuState state = MAIN_SCREEN;
WorkMode workMode = MODE_MANUAL;
GlassStatus glassState[NUM_GLASSES];

// Неблокирующие таймеры и переменные управления
unsigned long lastActivityTime = 0;
unsigned long lastEncoderMoveTime = 0;
unsigned long lastButtonDebounceTime = 0;
unsigned long pressTime = 0;
unsigned long lastReleaseTime = 0;
unsigned long lastToastTime = 0;
unsigned long toastScreenTimer = 0;
unsigned long toastAudioTimer = 0;
unsigned long glassPlacementTime[NUM_GLASSES] = {0};
unsigned long glassStaleTimer[NUM_GLASSES] = {0};
unsigned long alertStopTimer[NUM_GLASSES] = {0};
unsigned long lastInviteTime = 0; // Наш фоновый таймер отсчета тишины Зазывалы

int menuSelector = 0;
int dosingGlassSelector = 0;
int currentGlass = 0;
int activeToastID = 1;
int currentStepperPos = 0;
int lastClk = HIGH;

bool lastButtonState = HIGH;
bool buttonPressProcessed = false;
bool isPress = false;
bool longPressTriggered = false;
bool waitSecondClick = false;
bool toastAudioPending = false;
bool isAlerting[NUM_GLASSES] = {false};
bool staleTimerArmed[NUM_GLASSES] = {false};
bool glassTimerArmed[NUM_GLASSES] = {false};
uint8_t toastSubItem = 0;

// ============================================================================
// 📑 БАЗОВЫЕ СТРОКОВЫЕ КОНСТАНТЫ В ПАМЯТИ PROGMEM
// ============================================================================
const char T_MANUAL[] PROGMEM = "РУЧНОЙ";
const char T_AUTO[] PROGMEM = "АВТОМАТ";
const char T_MENU[] PROGMEM = "МЕНЮ";
const char T_HOME_BASE[] PROGMEM = "ДОМ.ТОЧКА";
const char T_BACK[] PROGMEM = "НАЗАД";
const char T_OK[] PROGMEM = "ОК";
const char T_DONE[] PROGMEM = "ГОТОВО";
const char T_FLUSH[] PROGMEM = "ПРОМЫВКА";
const char T_RESET[] PROGMEM = "СБРОС";
const char T_ON[] PROGMEM = "ВКЛ";
const char T_OFF[] PROGMEM = "ВЫКЛ";

const char M_CLICK_POUR[] PROGMEM = "Клик для налива";
const char M_WAIT_GLASS[] PROGMEM = "Жду рюмку...";
const char M_NO_GLASS[] PROGMEM = "Нет рюмок!";
const char M_NO_NEW[] PROGMEM = "Нет новых рюмок";
const char M_PLACE_GLASS[] PROGMEM = "Поставьте рюмку";
const char M_REMOVED[] PROGMEM = "Рюмка убрана!";
const char M_ABORT[] PROGMEM = "Налив прерван";
const char M_CALIB_TITLE[] PROGMEM = "КАЛИБРОВКА";
const char M_INVITE_CALL[] PROGMEM = "Пора накатить!.."; 
const char M_RESET_CONF[] PROGMEM = "Сбросить память?";
const char M_FLUSH_CONF[] PROGMEM = "Включить промывку?";
const char T_ROT_HINT[] PROGMEM = "Поворот - Выбор";

// Названия пунктов главного меню
const char M0[] PROGMEM = "РЕЖИМ";
const char M1[] PROGMEM = "УГЛЫ";
const char M2[] PROGMEM = "ОБЪЕМ";
const char M3[] PROGMEM = "ЗАЗЫВАЛА"; // Пункт меню вместо Рулетки
const char M4[] PROGMEM = "ТАМАДА";
const char M5[] PROGMEM = "ПИНАТЕЛЬ";
const char M6[] PROGMEM = "ПОМПА";
const char M7[] PROGMEM = "СВЕТ";
const char M8[] PROGMEM = "ЗАДЕРЖКА";
const char M9[] PROGMEM = "ПРОЛИВ";
const char M10[] PROGMEM = "СБРОС";
const char* const MENU[] PROGMEM = {M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, M10};

const char S_EDIT[] PROGMEM = " ЕДИТ ПОРЦИЙ ";
const char S_MAN[] PROGMEM = " РУЧНОЙ РЕЖИМ ";
const char S_AUTO[] PROGMEM = " АВТО РЕЖИМ ";
const char S_ROUL[] PROGMEM = " ЗАЗЫВАЛА ";
const char S_HINT1[] PROGMEM = "Крути-мл|Клик-След";
const char S_STEP[] PROGMEM = "ШАГИ:";
const char S_MS[] PROGMEM = " мс";
const char S_SEC[] PROGMEM = " сек";
const char S_ML[] PROGMEM = " мл";
const char S_VOL[] PROGMEM = "ОБЪЕМ (ОБЩИЙ)";
const char S_T_OUT[] PROGMEM = "ТАЙМАУТ";
const char S_SND[] PROGMEM = "ЗВУК";
const char S_PNK[] PROGMEM = "ПИНОК";
const char S_BRG[] PROGMEM = "ЯРКОСТЬ";
const char S_MIN[] PROGMEM = " мин"; 
const char S_VOL_M[] PROGMEM = "ГРОМКОСТЬ";
const char S_PAUSE[] PROGMEM = "ПАУЗА";
const char S_DELAY[] PROGMEM = "ОТСРОЧКА";
const char S_STRT[] PROGMEM = " СТАРТ ";
const char S_TEST[] PROGMEM = "Зажмите для теста";
const char S_SHG[] PROGMEM = " шг";
const char S_KD[] PROGMEM = " кд";

// ============================================================================
// ⚙️ НИЗКОУРОВНЕВЫЕ ФУНКЦИИ ЖЕЛЕЗА
// ============================================================================
bool readGlassSensor(int index) {
  if (index < 0 || index >= NUM_GLASSES) return HIGH;
  int pin = pinSensors[index];
  if (pin == A6 || pin == A7) {
    return (analogRead(pin) < 400) ? LOW : HIGH;
  }
  return digitalRead(pin);
}

void stepMotor(int dir) {
  static int currentStep = 0;
  currentStep += dir;
  if (currentStep > 7) currentStep = 0;
  if (currentStep < 0) currentStep = 7;
  for (int pin = 0; pin < 4; pin++) {
    digitalWrite(pinStepper[pin], bitRead(stepperSequence[currentStep], pin));
  }
}

void disableMotorOutputs() {
  for (int pin = 0; pin < 4; pin++) { digitalWrite(pinStepper[pin], LOW); }
}

void moveStepperTo(int targetPos) {
  if (currentStepperPos == targetPos) return;
  resetSleepTimer();
  while (currentStepperPos != targetPos) {
    int dir = (targetPos - currentStepperPos > 0) ? 1 : -1;
    stepMotor(dir);
    currentStepperPos += dir;
    delayMicroseconds(2000);
  }
  disableMotorOutputs();
}

void homeStepper() {
  disableMotorOutputs();
  currentStepperPos = set.homePos;
}

void sendMP3Raw(uint8_t command, uint8_t dat1, uint8_t dat2) {
  uint8_t mp3Buf[8] = {0x7E, 0xFF, 0x06, command, 0x00, dat1, dat2, 0xEF};
  for (uint8_t i = 0; i < 8; i++) { Serial.write(mp3Buf[i]); }
}

void setDefaultSettings() {
  set.homePos = 0;
  set.mlTimeMs = 150;
  set.shotVolume = 30;
  set.ledBrightness = 60;
  set.sensorDelayMs = 1000;
  set.mp3Volume = 15;
  set.toastPauseS = 1;
  set.toastDelayS = 15;
  set.startDelayS = 1;
  set.inviteTimeM = 20;     // По умолчанию напоминать раз в 20 минут
  set.invitePlaceholder = 0;
  set.staleTimeS = 0;
  set.staleAlertS = 3;
  set.savedMode = 0;

  int defaultAngles[NUM_GLASSES] = {400, 800, 1200, 1600, 2000, 2400};
  for (int i = 0; i < NUM_GLASSES; i++) {
    set.shotPos[i] = defaultAngles[i];
    set.shotVolumeIndividual[i] = set.shotVolume;
    set.shotVolumeCustomized[i] = false;
  }
}

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(pinStepper[i], OUTPUT);
    digitalWrite(pinStepper[i], LOW);
  }
  for (int i = 0; i < NUM_GLASSES; i++) {
    int pin = pinSensors[i];
    if (pin != A6 && pin != A7) { pinMode(pin, INPUT_PULLUP); }
  }
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(MP3_BUSY_PIN, INPUT_PULLUP);

  lastClk = digitalRead(ENCODER_CLK);
  lastButtonState = digitalRead(ENCODER_SW);

  EEPROM.get(0, set);
  if (set.mlTimeMs < 10 || set.mlTimeMs > 2000) {
    setDefaultSettings();
    EEPROM.put(0, set);
  }

  if (set.savedMode == 0) workMode = MODE_MANUAL;
  else workMode = MODE_AUTOMATIC;

  for (int i = 0; i < NUM_GLASSES; i++) { glassState[i] = EMPTY_SPACE; }

  Serial.begin(9600);
  delay(500);
  if (set.mp3Volume > 0) { sendMP3Raw(0x06, 0, set.mp3Volume); delay(100); }

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_GLASSES);
  FastLED.setBrightness(set.ledBrightness);
  FastLED.clear(); FastLED.show();

  oled.init(); oled.clear();
  updateDisplay();
  resetSleepTimer();
  homeStepper();
  lastInviteTime = millis();
  randomSeed(analogRead(A3) + analogRead(A4) + micros());
}
//------------------------------------------------------------------------------------
//--------------------- 2-я часть. КОНЕЦ 
//------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
//--------------------- 3-я часть. НАЧАЛО 
//------------------------------------------------------------------------------------
void resetSleepTimer() { 
  lastActivityTime = millis(); 
  lastInviteTime = millis(); // Любое действие пользователя или налив сбрасывают таймер тишины Зазывалы
}

void wakeUpSystem() {
  if (state == SLEEP_MODE) {
    state = MAIN_SCREEN; oled.init(); updateDisplay(); resetSleepTimer(); delay(100);
  }
}

void loop() {
  // Проверяем, есть ли на столе хоть одна налитая рюмка
  bool hasAnyPouredGlass = false;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (glassState[i] == FILLED_GLASS) {
      hasAnyPouredGlass = true;
      break;
    }
  }

  if (hasAnyPouredGlass && state == MAIN_SCREEN) {
    resetSleepTimer();
  }

  if (state != SLEEP_MODE && state != POURING && state != PUMP_FLUSH && state != TOAST_SCREEN) {
    if (millis() - lastActivityTime > SLEEP_TIMEOUT_MS) {
      state = SLEEP_MODE; disableMotorOutputs(); oled.clear(); FastLED.clear(); FastLED.show();
    }
  }

  // ОБРАБОТКА ЭКРАНА ТОСТА / ЗАЗЫВАНИЯ
  if (state == TOAST_SCREEN) {
    if (toastAudioPending) {
      if (millis() - toastAudioTimer > ((unsigned long)set.toastPauseS * 1000UL)) {
        toastAudioPending = false;
        // Если громкость больше нуля, включаем строго ROULETTE_TRACK_ID (Папка 1, трек 1)
        if (set.mp3Volume > 0) { sendMP3Raw(0x0F, SYSTEM_SOUND_FOLDER, ROULETTE_TRACK_ID); delay(250); }
      }
    }

    // Пока активен экран зазывания, крутим световую анимацию на пустом столе
    if (!hasAnyPouredGlass && set.inviteTimeM > 0) {
      int flashPhase = (millis() / 100) % NUM_GLASSES;
      FastLED.clear();
      leds[flashPhase] = COLOR_ACTIVE;
      FastLED.show();
    }

    if (!toastAudioPending && digitalRead(MP3_BUSY_PIN) == HIGH) {
      state = MAIN_SCREEN; FastLED.clear(); FastLED.show(); updateDisplay(); resetSleepTimer(); delay(200); return;
    }
    if (millis() - toastScreenTimer > ((unsigned long)set.toastDelayS * 1000UL)) {
      sendMP3Raw(0x16, 0, 0); delay(50); state = MAIN_SCREEN; FastLED.clear(); FastLED.show(); updateDisplay(); resetSleepTimer(); delay(200); return;
    }
    if (digitalRead(ENCODER_SW) == LOW || digitalRead(ENCODER_CLK) == LOW) {
      sendMP3Raw(0x16, 0, 0); delay(100); state = MAIN_SCREEN; FastLED.clear(); FastLED.show(); updateDisplay(); resetSleepTimer(); delay(300); return;
    }
    return;
  }

  if (state == SLEEP_MODE) {
    if (digitalRead(ENCODER_SW) == LOW || digitalRead(ENCODER_CLK) == LOW) wakeUpSystem();
    for (int i = 0; i < NUM_GLASSES; i++) if (readGlassSensor(i) == LOW) { wakeUpSystem(); break; }
    delay(20); return;
  }

  // ==================== ЛОГИКА АКТИВНОГО ЗАЗЫВАЛЫ ====================
  // Работает на главном экране, если уставка минут > 0 и на столе НЕТ налитых рюмок
  if (state == MAIN_SCREEN && set.inviteTimeM > 0 && !hasAnyPouredGlass) {
    if (millis() - lastInviteTime >= ((unsigned long)set.inviteTimeM * 60000UL)) {
      lastInviteTime = millis();
      lastToastTime = millis();
      state = TOAST_SCREEN; 
      toastScreenTimer = millis(); 
      toastAudioTimer = millis(); 
      toastAudioPending = true; 
      
      oled.clear(); 
      // Вместо прокрутки рулетки просто выводим текст
      printStr(M_INVITE_CALL, 3, 1, true); 
    }
  }

  // Обработка вращения энкодера (сбросит таймер Зазывалы)
  int currentClk = digitalRead(ENCODER_CLK);
  if (currentClk != lastClk && currentClk == LOW) {
    resetSleepTimer();
    lastEncoderMoveTime = millis();
    if (digitalRead(ENCODER_DT) != currentClk) handleEncoderTurn(true); else handleEncoderTurn(false);
  }
  lastClk = currentClk;

  // ОБРАБОТКА КНОПКИ С АНТИДРЕБЕЗГОМ (сбросит таймер Зазывалы)
  bool currentButtonState = digitalRead(ENCODER_SW);
  if (currentButtonState != lastButtonState) {
    lastButtonDebounceTime = millis();
  }

  if ((millis() - lastButtonDebounceTime) > BUTTON_DEBOUNCE_MS) {
    if (currentButtonState == LOW && !buttonPressProcessed) {
      buttonPressProcessed = true;
      resetSleepTimer();
      if (millis() - lastEncoderMoveTime > 300) {
        pressTime = millis();
        isPress = true;
        longPressTriggered = false;
      }
    }
    else if (currentButtonState == HIGH && buttonPressProcessed) {
      buttonPressProcessed = false;
      if (isPress && !longPressTriggered) {
        unsigned long holdDuration = millis() - pressTime;
        if (holdDuration >= 80 && holdDuration < 600) {
          if (waitSecondClick && (millis() - lastReleaseTime < (unsigned long)DOUBLE_CLICK_TIMEOUT_MS)) {
            waitSecondClick = false;
            handleDoubleBlank();
          } else {
            waitSecondClick = true;
            lastReleaseTime = millis();
          }
        }
      }
      isPress = false;
    }
  }

  if (isPress && !longPressTriggered && (millis() - pressTime > 600)) {
    longPressTriggered = true;
    waitSecondClick = false;
    handleLongPress();
  }

  if (waitSecondClick && (millis() - lastReleaseTime >= (unsigned long)DOUBLE_CLICK_TIMEOUT_MS)) {
    waitSecondClick = false;
    if (state == MAIN_SCREEN && workMode == MODE_MANUAL) {
      bool physicsGlassPresent = false;
      for (int g = 0; g < NUM_GLASSES; g++) {
        if (readGlassSensor(g) == LOW && glassState[g] == EMPTY_GLASS) { physicsGlassPresent = true; break; }
      }
      if (!physicsGlassPresent) {
        oled.clear(); printStr(M_NO_NEW, 3, 1, true); delay(1500); updateDisplay();
      } else {
        handleShortClick();
      }
    } else {
      handleShortClick();
    }
  }

  lastButtonState = currentButtonState;

  if (state == CAL_PUMP && currentButtonState == LOW && !longPressTriggered) {
    static unsigned long calibPressStart = 0;
    static bool calibPressActive = false;
    if (!calibPressActive) { calibPressStart = millis(); calibPressActive = true; }
    if (millis() - calibPressStart > 400) { runPumpCalibration(); calibPressActive = false; }
  }

  bool allGlassesDisabled = true;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (set.shotVolumeIndividual[i] > 0) { allGlassesDisabled = false; break; }
  }

//--------------------------------------------------------------------------------------------------------- 3-2 часть. НАЧАЛО -------------------------------------------------------------------------------------------------

  // ==================== ОПРОС ДАТЧИКОВ СТОЛА И "ПИНАТЕЛЬ" ====================
  bool tableChanged = false;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (allGlassesDisabled && state == MAIN_SCREEN) {
      if (leds[i] != CRGB::Black) { leds[i] = CRGB::Black; tableChanged = true; }
      glassTimerArmed[i] = false; glassPlacementTime[i] = 0;
      glassState[i] = EMPTY_SPACE;
      glassStaleTimer[i] = 0; alertStopTimer[i] = 0; isAlerting[i] = false; staleTimerArmed[i] = false;
      continue;
    }

    bool sensorState = readGlassSensor(i);
    if (sensorState == HIGH) {
      // РЮМКИ ФИЗИЧЕСКИ НЕТ НА СТОЛЕ: Полный сброс всех флагов и таймеров для этой позиции
      if (glassTimerArmed[i] || glassPlacementTime[i] != 0) { glassTimerArmed[i] = false; glassPlacementTime[i] = 0; tableChanged = true; }
      glassStaleTimer[i] = 0; alertStopTimer[i] = 0; isAlerting[i] = false; staleTimerArmed[i] = false;

      // Если стакан убрали, а он был налит — запускаем Тамаду строго один раз при смене статуса!
      if (glassState[i] == FILLED_GLASS) {
        glassState[i] = EMPTY_SPACE;
        tableChanged = true;
        resetSleepTimer(); // Установка новой рюмки или уборка старой сбросит таймер Зазывалы!

        if (state == MAIN_SCREEN) {
          bool triggerToast = false;
          bool anyFullGlassLeft = false;
          for (int j = 0; j < NUM_GLASSES; j++) {
            if (j != i && readGlassSensor(j) == LOW && glassState[j] == FILLED_GLASS) {
              anyFullGlassLeft = true;
              break;
            }
          }
          if (!anyFullGlassLeft) triggerToast = true;

          // Запускаем тост "Тамады", только если "Зазывала" сейчас не играет призывный трек
          if (triggerToast && (millis() - lastToastTime >= ((unsigned long)set.toastDelayS * 1000UL))) {
            lastToastTime = millis(); 
            activeToastID = random(1, TOTAL_TOAST_TRACKS + 1);
            state = TOAST_SCREEN; toastScreenTimer = millis(); toastAudioTimer = millis(); toastAudioPending = true; showToastOnDisplay();
          }
        }
      } else {
        glassState[i] = EMPTY_SPACE;
      }

      // Светодиод пустого места
      if (set.shotVolumeIndividual[i] == 0 && state == MAIN_SCREEN) {
        if (leds[i] != CRGB::Black) { leds[i] = CRGB::Black; tableChanged = true; }
      } else if (state == MAIN_SCREEN) {
        CRGB targetStandby = COLOR_STANDBY;
        targetStandby.nscale8_video(BRIGHT_STANDBY);
        if (leds[i] != targetStandby) { leds[i] = targetStandby; tableChanged = true; }
      }
    } else {
      // РЮМКА ФИЗИЧЕСКИ СТОИТ НА СТОЛЕ (Датчик LOW)
      if (state == MAIN_SCREEN) {
        if (set.shotVolumeIndividual[i] == 0) {
          // Место программно отключено (0 мл)
          glassState[i] = DISABLED_SPACE;
          CRGB targetDisabled = COLOR_DISABLED;
          targetDisabled.nscale8_video(BRIGHT_DISABLED);
          if (leds[i] != targetDisabled) { leds[i] = targetDisabled; tableChanged = true; }
          glassTimerArmed[i] = false;
        } else {
          if (glassState[i] == DISABLED_SPACE) { glassState[i] = FILLED_GLASS; tableChanged = true; }

          // Если стакан только что поставили на пустое место — он переходит в EMPTY_GLASS
          if (glassState[i] == EMPTY_SPACE) {
            glassState[i] = EMPTY_GLASS;
            tableChanged = true;
            resetSleepTimer(); // Установка рюмки мгновенно сбросит таймер тишины Зазывалы!
          }

          // Ветка работы налитой рюмки (Пинатель) — работает независимо!
          if (glassState[i] == FILLED_GLASS) {
            if (!staleTimerArmed[i] && !isAlerting[i] && set.staleTimeS > 0) {
              glassStaleTimer[i] = millis();
              staleTimerArmed[i] = true;
            }

            if (set.staleTimeS > 0 && staleTimerArmed[i]) {
              if (!isAlerting[i]) {
                if (millis() - glassStaleTimer[i] >= ((unsigned long)set.staleTimeS * 1000UL)) {
                  isAlerting[i] = true;
                  alertStopTimer[i] = millis();
                  if (set.mp3Volume > 0 && digitalRead(MP3_BUSY_PIN) == HIGH) {
                    sendMP3Raw(0x0F, (byte)SYSTEM_SOUND_FOLDER, STALE_SOUND_TRACK_ID);
                  }
                }
                CRGB targetPoured = COLOR_POURED;
                targetPoured.nscale8_video(BRIGHT_POURED);
                if (leds[i] != targetPoured) { leds[i] = targetPoured; tableChanged = true; }
              } else {
                if (millis() - alertStopTimer[i] < ((unsigned long)set.staleAlertS * 1000UL)) {
                  int flashPhase = (millis() / 150) % 6;
                  CRGB nextColor;
                  if (flashPhase == 0 || flashPhase == 2 || flashPhase == 4) nextColor = CRGB::Black;
                  else {
                    CRGB targetPoured = COLOR_POURED;
                    targetPoured.nscale8_video(BRIGHT_POURED);
                    nextColor = targetPoured;
                  }
                  if (leds[i] != nextColor) { leds[i] = nextColor; FastLED.show(); }
                } else {
                  isAlerting[i] = false; alertStopTimer[i] = 0;
                  glassStaleTimer[i] = millis();
                  CRGB targetPoured = COLOR_POURED;
                  targetPoured.nscale8_video(BRIGHT_POURED);
                  if (leds[i] != targetPoured) { leds[i] = targetPoured; tableChanged = true; }
                }
              }
            } else {
              CRGB targetPoured = COLOR_POURED;
              targetPoured.nscale8_video(BRIGHT_POURED);
              if (leds[i] != targetPoured) { leds[i] = targetPoured; tableChanged = true; }
            }
          }
          else if (glassState[i] == EMPTY_GLASS) {
            // Ветка ожидания автоматического налива пустой рюмки
            CRGB targetActive = COLOR_ACTIVE;
            targetActive.nscale8_video(BRIGHT_ACTIVE);
            if (leds[i] != targetActive) { leds[i] = targetActive; tableChanged = true; }

            resetSleepTimer();
            if (!glassTimerArmed[i]) { glassPlacementTime[i] = millis(); glassTimerArmed[i] = true; tableChanged = true; }
          }
        }
      }
    }
  }
  if (tableChanged) FastLED.show();
  if (tableChanged && state == MAIN_SCREEN) updateDisplay();

  // ИНТЕЛЛЕКТУАЛЬНЫЙ АВТОМАТ НАЛИВА И ПАРКОВКИ В LOOP
  if (state == MAIN_SCREEN && !allGlassesDisabled) {
    bool needPour = false;

    if (workMode == MODE_AUTOMATIC) {
      for (int i = 0; i < NUM_GLASSES; i++) {
        if (readGlassSensor(i) == LOW && glassState[i] == EMPTY_GLASS && glassTimerArmed[i] && set.shotVolumeIndividual[i] > 0) {
          if (millis() - glassPlacementTime[i] >= (unsigned long)set.sensorDelayMs) {
            glassTimerArmed[i] = false;
            needPour = true;
            break;
          }
        }
      }
    }

    if (needPour) {
      resetSleepTimer();
      checkAndPour();
    }
    else if (currentStepperPos != set.homePos) {
      // Неблокирующее возвращение башни домой (пошагово, без задержек задержек delay)
      static unsigned long lastStepMicros = 0;
      if (micros() - lastStepMicros >= 2000) {
        lastStepMicros = micros();
        int dir = (set.homePos - currentStepperPos > 0) ? 1 : -1;
        stepMotor(dir);
        currentStepperPos += dir;

        if (workMode == MODE_AUTOMATIC) {
          for (int i = 0; i < NUM_GLASSES; i++) {
            if (readGlassSensor(i) == LOW && glassState[i] == EMPTY_GLASS && set.shotVolumeIndividual[i] > 0 && !glassTimerArmed[i]) {
              glassPlacementTime[i] = millis();
              glassTimerArmed[i] = true;
              CRGB targetActive = COLOR_ACTIVE;
              targetActive.nscale8_video(BRIGHT_ACTIVE);
              leds[i] = targetActive;
              FastLED.show();
            }
          }
        }
        if (currentStepperPos == set.homePos) disableMotorOutputs();
      }
    }
  }
} // <- Финальная закрывающая скобка функции loop()
//------------------------------------------------------------------------------------
//--------------------- 3-я часть. КОНЕЦ 
//------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------- 4-я часть. НАЧАЛО -------------------------------------------------------------------------------------------------
// [ИСПРАВЛЕНО]: Честный подсчет символов UTF-8 (русских букв) для идеального центрирования
void printStr(const char* str, uint8_t y, uint8_t scale, bool isProgmem) {
  uint8_t len = 0;

  if (isProgmem) {
    const char* p = str;
    byte c;
    while ((c = pgm_read_byte(p++))) {
      if ((c & 0xC0) != 0x80) len++;
    }
  } else {
    const char* p = str;
    byte c;
    while ((c = *p++)) {
      if ((c & 0xC0) != 0x80) len++;
    }
  }

  uint8_t letterW = (scale == 1) ? 6 : ((scale == 2) ? 12 : 18);
  uint8_t x = (128 - (len * letterW)) / 2;
  if (x > 120) x = 0;

  oled.setCursor(x, y);
  oled.setScale(scale);

  if (isProgmem) {
    // Исправлено: корректный размер буфера и условие выхода из цикла
    char buf[32];
    uint8_t i = 0;
    const char* p = str;
    char c;
    while ((c = pgm_read_byte(p++)) && i < 31) {
      buf[i++] = c;
    }
    buf[i] = '\0';
    oled.print(buf);
  } else {
    oled.print(str);
  }
}

void printStr(const __FlashStringHelper* str, uint8_t y, uint8_t scale, bool isProgmem) {
  uint8_t len = 0;
  const char* p = (const char*)str;
  byte c;

  while ((c = pgm_read_byte(p++))) {
    if ((c & 0xC0) != 0x80) len++;
  }

  uint8_t letterW = (scale == 1) ? 6 : ((scale == 2) ? 12 : 18);
  uint8_t x = (128 - (len * letterW)) / 2;
  if (x > 120) x = 0;

  oled.setCursor(x, y);
  oled.setScale(scale);
  oled.print(str);
}

// Функция для центрированной печати строки с рюмками
void printGlassesRow(int startIdx, uint8_t y, bool isEditing, int editIdx) {
  char buffer[32];
  uint8_t pos = 0;

  for (int i = startIdx; i < startIdx + 3; i++) {
    if (isEditing && editIdx == i) {
      buffer[pos++] = '[';
    } else {
      buffer[pos++] = ' ';
    }

    if (set.shotVolumeIndividual[i] == 0) {
      buffer[pos++] = 'B';
      buffer[pos++] = 'b';
      buffer[pos++] = 'I';
      buffer[pos++] = 'K';
    } else {
      int val = set.shotVolumeIndividual[i];
      if (val < 100) buffer[pos++] = ' ';
      if (val < 10) buffer[pos++] = ' ';
      pos += sprintf(&buffer[pos], "%d", val);
    }

    if (isEditing && editIdx == i) {
      buffer[pos++] = ']';
    } else {
      buffer[pos++] = ' ';
    }

    if (i < startIdx + 2) {
      buffer[pos++] = '|';
    }
  }
  buffer[pos] = '\0';

  uint8_t len = strlen(buffer);
  uint8_t x = (128 - (len * 6)) / 2;
  if (x > 120) x = 0;

  oled.setCursor(x, y);
  oled.print(buffer);
}

// Функция вывода "ОК" с жесткой очисткой
void showSaveAlert() {
  oled.clear();
  printStr(T_OK, 3, 2, true);

  unsigned long clearTimer = millis();
  while (millis() - clearTimer < 500) {
    waitSecondClick = false;
    isPress = false;
    longPressTriggered = false;
    pressTime = millis();
    lastReleaseTime = millis();
    delay(5);
  }
  oled.clear();
}

void performReset() {
  oled.clear(); oled.setScale(1);
  printStr(T_DONE, 3, 2, true);
  setDefaultSettings();
  workMode = MODE_MANUAL;

  for (int i = 0; i < NUM_GLASSES; i++) {
    glassState[i] = EMPTY_SPACE;
    set.shotVolumeCustomized[i] = false;
  }

  delay(1200);
  homeStepper();
  FastLED.clear(); FastLED.show();
  oled.init();

  state = ROOT_MENU;
  menuSelector = 10;
  updateDisplay();
}

void runSystemFlush() {
  int chosenGlass = -1;
  for (int g = 0; g < NUM_GLASSES; g++) { if (readGlassSensor(g) == LOW) { chosenGlass = g; break; } }
  if (chosenGlass == -1) {
    oled.clear();
    printStr(M_PLACE_GLASS, 3, 1, true);
    delay(2000);
    state = ROOT_MENU;
    updateDisplay();
    return;
  }
  
  // Исправлено: ждем, пока пользователь отпустит кнопку после входа в меню промывки
  while (digitalRead(ENCODER_SW) == LOW) { delay(10); }
  delay(50); 

  oled.clear();
  printStr(T_FLUSH, 1, 1, true);
  moveStepperTo(set.shotPos[chosenGlass]); delay(600);
  leds[chosenGlass] = CRGB::White; FastLED.show();
  digitalWrite(PUMP_PIN, HIGH);
  bool removed = false;
  while (digitalRead(ENCODER_SW) == HIGH) {
    if (readGlassSensor(chosenGlass) == HIGH) { removed = true; break; }
    delay(30);
  }
  digitalWrite(PUMP_PIN, LOW);
  if (removed) {
    leds[chosenGlass] = CRGB::Black; FastLED.show();
    oled.clear(); printStr(M_REMOVED, 2, 1, true); printStr(M_ABORT, 4, 1, true);
    delay(1500);
  } else {
    leds[chosenGlass] = CRGB::Green; FastLED.show(); delay(500);
    leds[chosenGlass] = CRGB::Black; FastLED.show();
    oled.clear(); printStr(T_DONE, 3, 2, true); delay(1500);
  }
  moveStepperTo(set.homePos);
  
  // Ждем отпускания кнопки на выходе, чтобы не было фантомных кликов в меню
  while (digitalRead(ENCODER_SW) == LOW) { delay(10); }
  
  state = ROOT_MENU;
  updateDisplay();
}

void runPumpCalibration() {
  int chosenGlass = -1;
  for (int g = 0; g < NUM_GLASSES; g++) { if (readGlassSensor(g) == LOW) { chosenGlass = g; break; } }
  if (chosenGlass == -1) {
    oled.clear();
    printStr(M_PLACE_GLASS, 3, 1, true);
    delay(2000);
    state = ROOT_MENU;
    updateDisplay();
    return;
  }
  oled.clear();
  printStr(M_CALIB_TITLE, 1, 1, true);
  moveStepperTo(set.shotPos[chosenGlass]); delay(600);
  unsigned long startTime = millis();
  digitalWrite(PUMP_PIN, HIGH);
  while (digitalRead(ENCODER_SW) == LOW) {
    if (readGlassSensor(chosenGlass) == HIGH) break;
    long t = millis() - startTime;
    oled.setCursor(31, 2); oled.setScale(3); oled.print(t);
    delay(10);
  }
  digitalWrite(PUMP_PIN, LOW);
  set.mlTimeMs = millis() - startTime;
  EEPROM.put(0, set);
  moveStepperTo(set.homePos);
  oled.clear();
  oled.setCursor(35, 3); oled.setScale(2);
  oled.print(set.mlTimeMs); oled.print(F("мс"));
  delay(2000);
  state = ROOT_MENU;
  updateDisplay();
}
//--------------------------------------------------------------------------------------------------------- 4-я часть. КОНЕЦ -------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
//--------------------- 5-я часть НАЧАЛО 
//------------------------------------------------------------------------------------
void handleEncoderTurn(bool isRight) {
  if (state == FAST_DOSING) {
    set.shotVolumeIndividual[dosingGlassSelector] += isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    set.shotVolumeIndividual[dosingGlassSelector] = constrain(set.shotVolumeIndividual[dosingGlassSelector], 0, MAX_POUR_VOLUME);
    set.shotVolumeCustomized[dosingGlassSelector] = true;
    updateDisplay();
    return;
  }

  if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      dosingGlassSelector += isRight ? 1 : -1;
      if (dosingGlassSelector > 4) dosingGlassSelector = 0;
      if (dosingGlassSelector < 0) dosingGlassSelector = 4;
    } else if (toastSubItem == 1) {
      if (dosingGlassSelector == 0) {
        set.mp3Volume += isRight ? 1 : -1;
        set.mp3Volume = constrain(set.mp3Volume, 0, 30);
        sendMP3Raw(0x06, 0, set.mp3Volume);
      } else if (dosingGlassSelector == 1) {
        set.toastPauseS += isRight ? 1 : -1;
        set.toastPauseS = constrain(set.toastPauseS, 0, 10);
      } else if (dosingGlassSelector == 2) {
        set.toastDelayS += isRight ? 1 : -1;
        set.toastDelayS = constrain(set.toastDelayS, 1, 30);
      } else if (dosingGlassSelector == 3) {
        set.startDelayS += isRight ? 1 : -1;
        set.startDelayS = constrain(set.startDelayS, 0, 10);
      }
    }
    updateDisplay();
    return;
  }
  
  if (state == SET_MODE) {
    if (toastSubItem == 0) {
      menuSelector += isRight ? 1 : -1;
      if (menuSelector > 2) menuSelector = 0;
      if (menuSelector < 0) menuSelector = 2;
    } else dosingGlassSelector = (dosingGlassSelector == 1) ? 0 : 1;
    updateDisplay();
    return;
  }

  if (state == SET_BASE_VOLUME) {
    int delta = isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    int targetVol = constrain(set.shotVolume + delta, MIN_POUR_VOLUME, MAX_POUR_VOLUME);
    set.shotVolume = targetVol;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (!set.shotVolumeCustomized[i]) set.shotVolumeIndividual[i] = targetVol;
    }
    updateDisplay();
    return;
  }

  if (state == SET_STALE_MENU) {
    dosingGlassSelector += isRight ? 1 : -1;
    if (dosingGlassSelector > 2) dosingGlassSelector = 0;
    if (dosingGlassSelector < 0) dosingGlassSelector = 2;
    updateDisplay();
    return;
  }

  if (state == SET_STALE_TIME) {
    if (dosingGlassSelector == 0) {
      set.staleTimeS += isRight ? STALE_TIME_STEP_S : -STALE_TIME_STEP_S;
      set.staleTimeS = constrain(set.staleTimeS, 0, MAX_STALE_TIME_S);
    } else if (dosingGlassSelector == 1) {
      set.staleAlertS += isRight ? STALE_ALERT_STEP_S : -STALE_ALERT_STEP_S;
      set.staleAlertS = constrain(set.staleAlertS, 1, MAX_STALE_ALERT_S);
    }
    updateDisplay();
    return;
  }

  // ОБНОВЛЕННАЯ НАСТРОЙКА ТАЙМАУТА ЗАЗЫВАЛЫ
  if (state == SET_INVITE_MENU) {
    if (toastSubItem == 0) {
      dosingGlassSelector += isRight ? 1 : -1;
      if (dosingGlassSelector > 1) dosingGlassSelector = 0;
      if (dosingGlassSelector < 0) dosingGlassSelector = 1;
    } else {
      set.inviteTimeM += isRight ? INVITE_TIME_STEP_M : -INVITE_TIME_STEP_M;
      set.inviteTimeM = constrain(set.inviteTimeM, 0, MAX_INVITE_TIME_M); // Разрешаем опускаться до 0
    }
    updateDisplay();
    return;
  }
  
  if (state == SUB_MENU_EDIT && menuSelector == 1) {
    if (toastSubItem == 0) {
      currentGlass += isRight ? 1 : -1;
      if (currentGlass > 7) currentGlass = 0;
      if (currentGlass < 0) currentGlass = 7;
    } else {
      int stepChange = isRight ? CALIBRATION_STEP_STEPS : -CALIBRATION_STEP_STEPS;
      if (currentGlass == 0) {
        set.homePos += stepChange;
        set.homePos = constrain(set.homePos, -200, 200);
        moveStepperTo(set.homePos);
      } else {
        int idx = currentGlass - 1;
        set.shotPos[idx] += stepChange;
        set.shotPos[idx] = constrain(set.shotPos[idx], 0, stepsPerRevolution);
        moveStepperTo(set.shotPos[idx]);
      }
    }
    updateDisplay();
    return;
  }
  
  if (state == SUB_MENU_EDIT && menuSelector != 1) {
    if (menuSelector == 7) {
      set.ledBrightness += isRight ? 5 : -5;
      set.ledBrightness = constrain(set.ledBrightness, 10, 255);
      FastLED.setBrightness(set.ledBrightness);
      FastLED.show();
    } else if (menuSelector == 8) {
      set.sensorDelayMs += isRight ? 100 : -100;
      set.sensorDelayMs = constrain(set.sensorDelayMs, 200, 3000);
    }
    updateDisplay();
    return;
  }
  
  if (state == RESET_PAGE || state == PUMP_FLUSH) {
    dosingGlassSelector = (dosingGlassSelector == 1) ? 0 : 1;
    updateDisplay();
    return;
  }

  if (state == MAIN_SCREEN) {
    int delta = isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    int baseVol = MIN_POUR_VOLUME;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (!set.shotVolumeCustomized[i] && readGlassSensor(i) == HIGH) {
        baseVol = set.shotVolumeIndividual[i];
        break;
      }
    }
    int targetVol = constrain(baseVol + delta, MIN_POUR_VOLUME, MAX_POUR_VOLUME);
    set.shotVolume = targetVol;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (readGlassSensor(i) == LOW && glassState[i] == FILLED_GLASS) continue;
      if (!set.shotVolumeCustomized[i]) { set.shotVolumeIndividual[i] = targetVol; }
      else if (set.shotVolumeIndividual[i] == targetVol) { set.shotVolumeCustomized[i] = false; }
    }
  } else if (state == ROOT_MENU) {
    menuSelector += isRight ? 1 : -1;
    if (menuSelector >= TOTAL_MENU_ITEMS) menuSelector = 0;
    if (menuSelector < 0) menuSelector = TOTAL_MENU_ITEMS - 1;
  }
  updateDisplay();
}
//------------------------------------------------------------------------------------
//--------------------- 5-я часть КОНЕЦ 
//------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------- 6-я часть НАЧАЛО -------------------------------------------------------------------------------------------------
void handleShortClick() {
  if (state == FAST_DOSING) {
    dosingGlassSelector++;
    if (dosingGlassSelector >= NUM_GLASSES) {
      EEPROM.put(0, set); showSaveAlert(); state = MAIN_SCREEN;
    }
    updateDisplay(); return;
  }

  if (state == MAIN_SCREEN) {
    if (workMode == MODE_MANUAL) checkAndPour();
  } else if (state == ROOT_MENU) {
    if (menuSelector == 0) { state = SET_MODE; menuSelector = 0; toastSubItem = 0; }
    else if (menuSelector == 1) { state = SUB_MENU_EDIT; currentGlass = 0; toastSubItem = 0; moveStepperTo(set.homePos); }
    else if (menuSelector == 2) { state = SET_BASE_VOLUME; }
    else if (menuSelector == 3) { state = SET_INVITE_MENU; dosingGlassSelector = 0; toastSubItem = 0; } // Переход на Зазывалу
    else if (menuSelector == 4) { state = SET_TOAST_TIME; dosingGlassSelector = 0; toastSubItem = 0; }
    else if (menuSelector == 5) { state = SET_STALE_MENU; dosingGlassSelector = 0; }
    else if (menuSelector == 6) { state = CAL_PUMP; }
    else if (menuSelector == 7) { state = SUB_MENU_EDIT; toastSubItem = 0; }
    else if (menuSelector == 8) { state = SUB_MENU_EDIT; toastSubItem = 0; }
    else if (menuSelector == 9) { state = PUMP_FLUSH; dosingGlassSelector = 0; }
    else if (menuSelector == 10) { state = RESET_PAGE; dosingGlassSelector = 0; }
  } else if (state == SET_BASE_VOLUME) {
    EEPROM.put(0, set); showSaveAlert(); state = ROOT_MENU; menuSelector = 2;
  } else if (state == SET_STALE_MENU) {
    if (dosingGlassSelector == 2) { state = ROOT_MENU; menuSelector = 5; }
    else { state = SET_STALE_TIME; }
  } else if (state == SET_STALE_TIME) {
    EEPROM.put(0, set); showSaveAlert(); state = SET_STALE_MENU;
  } else if (state == SUB_MENU_EDIT) {
    if (menuSelector == 1) {
      if (toastSubItem == 0) {
        if (currentGlass == 7) { state = ROOT_MENU; menuSelector = 1; }
        else toastSubItem = 1;
      } else { EEPROM.put(0, set); showSaveAlert(); toastSubItem = 0; }
    } else { EEPROM.put(0, set); showSaveAlert(); state = ROOT_MENU; }
  } else if (state == SET_MODE) {
    if (toastSubItem == 0) {
      if (menuSelector == 2) { state = ROOT_MENU; menuSelector = 0; } // Исправлен индекс выхода
      else { toastSubItem = 1; dosingGlassSelector = 0; }
    } else if (toastSubItem == 1) {
      if (dosingGlassSelector == 0) {
        if (menuSelector == 0) workMode = MODE_MANUAL;
        else if (menuSelector == 1) workMode = MODE_AUTOMATIC;
        state = MAIN_SCREEN; set.savedMode = (menuSelector == 0) ? 0 : 1; EEPROM.put(0, set); showSaveAlert();
        menuSelector = 0; toastSubItem = 0;
      } else toastSubItem = 0;
    }
  } else if (state == SET_INVITE_MENU) {
    // Выход и сохранение настроек Зазывалы по клику
    if (toastSubItem == 0) {
      if (dosingGlassSelector == 1) { state = ROOT_MENU; menuSelector = 3; toastSubItem = 0; }
      else toastSubItem = 1;
    } else { EEPROM.put(0, set); showSaveAlert(); toastSubItem = 0; }
  } else if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      if (dosingGlassSelector == 4) { state = ROOT_MENU; menuSelector = 4; }
      else toastSubItem = 1;
    } else { EEPROM.put(0, set); showSaveAlert(); toastSubItem = 0; }
  } else if (state == CAL_PUMP) { EEPROM.put(0, set); showSaveAlert(); state = ROOT_MENU; menuSelector = 6; }
  else if (state == PUMP_FLUSH) { if (dosingGlassSelector == 1) runSystemFlush(); else { state = ROOT_MENU; menuSelector = 9; } }
  else if (state == RESET_PAGE) { if (dosingGlassSelector == 1) performReset(); else { state = ROOT_MENU; menuSelector = 10; } }
  updateDisplay();
}

void handleLongPress() {
  if (state == FAST_DOSING) {
    EEPROM.put(0, set);
    showSaveAlert();
    state = MAIN_SCREEN;
    updateDisplay();
    return;
  }

  FastLED.setBrightness(set.ledBrightness);
  FastLED.show();
  sendMP3Raw(0x06, 0, set.mp3Volume);

  if (state == MAIN_SCREEN || state == ROOT_MENU) {
    state = (state == MAIN_SCREEN) ? ROOT_MENU : MAIN_SCREEN;
    moveStepperTo(set.homePos);
  } else {
    if (toastSubItem > 0) {
      toastSubItem = 0;
      if (state == SUB_MENU_EDIT && menuSelector == 1) {
        if (currentGlass == 0) moveStepperTo(set.homePos);
        else moveStepperTo(set.shotPos[currentGlass - 1]);
      }
    } else {
      if (state == SET_STALE_TIME) {
        state = SET_STALE_MENU;
      } else if (state == SET_STALE_MENU) {
        state = ROOT_MENU; menuSelector = 5;
      } else {
        state = ROOT_MENU;
      }
    }
    moveStepperTo(set.homePos);
  }
  updateDisplay();
}

void handleDoubleBlank() {
  if (state == MAIN_SCREEN) {
    state = FAST_DOSING;
    dosingGlassSelector = 0;
  }
  else if (state == FAST_DOSING) {
    EEPROM.get(0, set);
    state = MAIN_SCREEN;
  }
  else if (state != ROOT_MENU && state != POURING && state != TOAST_SCREEN) {
    state = MAIN_SCREEN;
    moveStepperTo(set.homePos);
  }
  updateDisplay();
}
//--------------------------------------------------------------------------------------------------------- 6-я часть КОНЕЦ -------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------- 7-я часть НАЧАЛО -------------------------------------------------------------------------------------------------
void checkAndPour() {
  MenuState prevState = state;
  state = POURING;
  bool glassFound = false;

  unsigned long localPlacementTime[NUM_GLASSES] = {0};
  bool localTimerArmed[NUM_GLASSES] = {false};

  for (int g = 0; g < NUM_GLASSES; g++) {
    rouletteActiveGlasses[g] = false;
  }

  if (workMode == MODE_ROULETTE) {
    int avail[NUM_GLASSES], availCnt = 0;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (readGlassSensor(i) == LOW && set.shotVolumeIndividual[i] > 0 && glassState[i] == EMPTY_GLASS) { avail[availCnt++] = i; }
    }
    if (availCnt == 0) {
      oled.clear(); printStr(M_NO_GLASS, 3, 1, true); delay(1200);
      state = prevState; updateDisplay(); return;
    }
    int target = avail[random(0, availCnt)];
    rouletteActiveGlasses[target] = true;
    oled.clear(); printStr(M_ROULETTE_SPIN, 3, 1, true);
    if (set.mp3Volume > 0) sendMP3Raw(0x0F, SYSTEM_SOUND_FOLDER, ROULETTE_TRACK_ID);
    int animSpeed = constrain(65 - (set.rouletteSpeed * 5), 5, 80);
    unsigned long start = millis();
    unsigned long duration = (unsigned long)set.rouletteSpinS * 1000UL;
    int step = 0;
    while (millis() - start < duration) {
      FastLED.clear(); leds[step % NUM_GLASSES] = CRGB::Red; FastLED.show();
      delay(animSpeed * 10);
      step++;
    }
    FastLED.clear(); FastLED.show();
  }

  while (true) {
    if (workMode != MODE_ROULETTE) {
      for (int i = 0; i < NUM_GLASSES; i++) {
        if (readGlassSensor(i) == HIGH) {
          glassState[i] = EMPTY_SPACE;
          localTimerArmed[i] = false; rouletteActiveGlasses[i] = false;
          CRGB targetStandby = COLOR_STANDBY;
          targetStandby.nscale8_video(BRIGHT_STANDBY);
          leds[i] = targetStandby;
          FastLED.show();
          continue;
        }

        if (readGlassSensor(i) == LOW && glassState[i] != FILLED_GLASS && glassState[i] != POURING_GLASS) {
          if (set.shotVolumeIndividual[i] == 0) {
            glassState[i] = DISABLED_SPACE;
            CRGB targetDisabled = COLOR_DISABLED;
            targetDisabled.nscale8_video(BRIGHT_DISABLED);
            if (leds[i] != targetDisabled) { leds[i] = targetDisabled; FastLED.show(); }
            localTimerArmed[i] = false; rouletteActiveGlasses[i] = false;
            continue;
          }

          if (glassState[i] == EMPTY_SPACE) {
            glassState[i] = EMPTY_GLASS;
          }

          if (!localTimerArmed[i] && !rouletteActiveGlasses[i]) {
            localPlacementTime[i] = millis();
            localTimerArmed[i] = true;
            CRGB targetActive = COLOR_ACTIVE;
            targetActive.nscale8_video(BRIGHT_ACTIVE);
            leds[i] = targetActive;
            FastLED.show();
          }
          if (localTimerArmed[i] && (millis() - localPlacementTime[i] >= (unsigned long)set.sensorDelayMs)) {
            localTimerArmed[i] = false;
            rouletteActiveGlasses[i] = true;
          }
        }
      }
    }

    int targetGlass = -1;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (rouletteActiveGlasses[i] && glassState[i] == EMPTY_GLASS) { targetGlass = i; break; }
    }

    if (targetGlass == -1) {
      bool anyPending = false;
      if (workMode != MODE_ROULETTE) {
        for (int i = 0; i < NUM_GLASSES; i++) {
          if (localTimerArmed[i]) { anyPending = true; break; }
        }
      }
      if (anyPending) { delay(10); continue; }
      break;
    }

    glassFound = true;
    int i = targetGlass;
    int vol = set.shotVolumeIndividual[i];
    oled.clear(); oled.setScale(1);
    oled.setCursor(19, 0);
    oled.print(F("НАЛИВАЮ В РЮМКУ "));
    oled.print(i + 1);

    glassState[i] = POURING_GLASS;
    moveStepperTo(set.shotPos[i]); delay(400);
    digitalWrite(PUMP_PIN, HIGH);

    unsigned long totalTime = ((unsigned long)vol * (unsigned long)set.mlTimeMs) / 50UL;
    unsigned long startTime = millis();
    bool removed = false;
    int lastMl = -1;

    while (millis() - startTime < totalTime) {
      if (readGlassSensor(i) == HIGH) { removed = true; break; }
      unsigned long elapsed = millis() - startTime;
      int percent = (elapsed * 100UL) / totalTime;

      int curMl = ((percent * vol) + 50) / 100;

      if (curMl != lastMl) {
        char buf[10];
        sprintf(buf, "%d мл", curMl);
        uint8_t len = strlen(buf);
        uint8_t x = (128 - (len * 12)) / 2;
        oled.setCursor(x, 4);
        oled.setScale(2);
        oled.print(buf);
        lastMl = curMl;
      }

      leds[i] = blend(CRGB::Blue, CRGB::Green, (percent * 255) / 100);
      FastLED.show();

      if (workMode != MODE_ROULETTE) {
        for (int j = 0; j < NUM_GLASSES; j++) {
          if (j == i) continue;

          if (readGlassSensor(j) == HIGH) {
            glassState[j] = EMPTY_SPACE;
            if (localTimerArmed[j]) {
              localTimerArmed[j] = false;
              CRGB targetStandby = COLOR_STANDBY;
              targetStandby.nscale8_video(BRIGHT_STANDBY);
              leds[j] = targetStandby; FastLED.show();
            }
            continue;
          }

          if (readGlassSensor(j) == LOW && glassState[j] != FILLED_GLASS) {
            if (set.shotVolumeIndividual[j] == 0) {
              glassState[j] = DISABLED_SPACE;
              CRGB targetDisabled = COLOR_DISABLED;
              targetDisabled.nscale8_video(BRIGHT_DISABLED);
              if (leds[j] != targetDisabled) { leds[j] = targetDisabled; FastLED.show(); }
              continue;
            }

            if (glassState[j] == EMPTY_SPACE) {
              glassState[j] = EMPTY_GLASS;
            }

            if (!localTimerArmed[j] && !rouletteActiveGlasses[j]) {
              localPlacementTime[j] = millis();
              localTimerArmed[j] = true;
              CRGB targetActive = COLOR_ACTIVE;
              targetActive.nscale8_video(BRIGHT_ACTIVE);
              leds[j] = targetActive;
              FastLED.show();
            }
            if (localTimerArmed[j] && (millis() - localPlacementTime[j] >= (unsigned long)set.sensorDelayMs)) {
              localTimerArmed[j] = false;
              rouletteActiveGlasses[j] = true;
            }
          }
        }
      }
      delay(10);
    }
    digitalWrite(PUMP_PIN, LOW);

    if (removed) {
      if (workMode == MODE_ROULETTE) { sendMP3Raw(0x16, 0, 0); delay(50); }
      glassState[i] = EMPTY_SPACE;
      CRGB targetStandby = COLOR_STANDBY;
      targetStandby.nscale8_video(BRIGHT_STANDBY);
      leds[i] = targetStandby; FastLED.show();
      oled.clear(); printStr(M_REMOVED, 2, 1, true); printStr(M_ABORT, 4, 1, true);
      delay(1200);
      rouletteActiveGlasses[i] = false;
      break;
    } else {
      if (workMode == MODE_ROULETTE) { sendMP3Raw(0x16, 0, 0); delay(50); }

      glassState[i] = FILLED_GLASS;

      CRGB targetPoured = COLOR_POURED;
      targetPoured.nscale8_video(BRIGHT_POURED);
      leds[i] = targetPoured; FastLED.show();
      if (workMode == MODE_ROULETTE) { roulettePouredFlags[i] = true; }
      delay(400);
    }
  }

  delay(100);
  if (!glassFound && workMode == MODE_MANUAL) {
    oled.clear(); printStr(M_NO_NEW, 3, 1, true); delay(1200);
  }

  state = MAIN_SCREEN;
  updateDisplay();
}
//--------------------------------------------------------------------------------------------------------- 7-я часть КОНЕЦ -------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------- 8-я часть НАЧАЛО -------------------------------------------------------------------------------------------------
void showToastOnDisplay() {
  state = TOAST_SCREEN;
  oled.clear();
  oled.rect(48, 22, 64, 42, OLED_STROKE);
  oled.line(64, 22, 78, 12);
  oled.line(64, 42, 78, 52);
  oled.line(78, 12, 78, 52);
  oled.circle(78, 32, 8, OLED_STROKE);
  oled.circle(78, 32, 16, OLED_STROKE);
  oled.rect(0, 0, 78, 64, OLED_CLEAR);
  oled.rect(48, 22, 64, 42, OLED_FILL);
  oled.line(64, 22, 78, 12);
  oled.line(64, 42, 78, 52);
  oled.line(78, 12, 78, 52);
  if (set.mp3Volume == 0) printStr(T_OFF, 7, 1, true);
}

const char M0[] PROGMEM = "РЕЖИМ";
const char M1[] PROGMEM = "УГЛЫ";
const char M2[] PROGMEM = "ОБЪЕМ";
const char M3[] PROGMEM = "РУЛЕТКА";
const char M4[] PROGMEM = "ТАМАДА";
const char M5[] PROGMEM = "ПИНАТЕЛЬ";
const char M6[] PROGMEM = "ПОМПА";
const char M7[] PROGMEM = "СВЕТ";
const char M8[] PROGMEM = "ЗАДЕРЖКА";
const char M9[] PROGMEM = "ПРОЛИВ";
const char M10[] PROGMEM = "СБРОС";
const char* const MENU[] PROGMEM = {M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, M10};
//--------------------------------------------------------------------------------------------------------- 8-я часть КОНЕЦ -------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------- 9-я часть НАЧАЛО -------------------------------------------------------------------------------------------------
void updateDisplay() {
  oled.clear();
  if (state == SLEEP_MODE) return;

  if (state == MAIN_SCREEN || state == FAST_DOSING) {
    if (state == FAST_DOSING) {
      printStr(F("ОБЪЕМ"), 0, 1, false);
    } else {
      if (workMode == MODE_MANUAL) printStr(T_MANUAL, 0, 1, true);
      else if (workMode == MODE_AUTOMATIC) printStr(T_AUTO, 0, 1, true);
      else printStr(T_ROULETTE, 0, 1, true);
    }

    //if (set.mp3Volume == 0) {
      //oled.setCursor(115, 0);
      //oled.print('M');
    //}

if (set.mp3Volume == 0) {
    oled.invertText(true);
    oled.setCursor(110, 0);
    oled.print(F(" M "));
    oled.invertText(false);
}

    oled.setScale(1);

    printGlassesRow(0, 2, (state == FAST_DOSING), dosingGlassSelector);
    printGlassesRow(3, 4, (state == FAST_DOSING), dosingGlassSelector);

    if (state == FAST_DOSING) {
      printStr(F("Круть/Клик"), 7, 1, false);
    } else {
      if (workMode == MODE_MANUAL || workMode == MODE_ROULETTE) {
        printStr(M_CLICK_POUR, 7, 1, true);
      } else {
        printStr(M_WAIT_GLASS, 7, 1, true);
      }
    }
  }
  else if (state == ROOT_MENU) {
    printStr(T_MENU, 0, 1, true);
    char* menuPtr = (char*)pgm_read_word(&MENU[menuSelector]);
    printStr(menuPtr, 3, 2, true);
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 1) {
    if (toastSubItem == 0) {
      printStr(F("КАЛИБРОВКА"), 1, 1, false);
      if (currentGlass == 0) {
        printStr(T_HOME_BASE, 3, 2, true);
      } else if (currentGlass <= 6) {
        oled.setCursor(45, 3);
        oled.setScale(2);
        oled.print(F("Р"));
        oled.print(currentGlass);
      } else {
        printStr(T_BACK, 3, 2, true);
      }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStr(F("КАЛИБРОВКА:"), 1, 1, false);
      oled.setCursor(30, 3);
      oled.setScale(3);
      if (currentGlass == 0) {
        oled.print(set.homePos);
      } else {
        oled.print(set.shotPos[currentGlass - 1]);
      }
      oled.setScale(1);
      oled.print(F(" шаг"));
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == SET_BASE_VOLUME) {
    printStr(F("ОБЪЕМ ПО УМОЛЧАНИЮ"), 1, 1, false);
    oled.setCursor(35, 3);
    oled.setScale(3);
    if (set.shotVolume == 0) {
      oled.print(F("ВЫКЛ"));
    } else {
      oled.print(set.shotVolume);
    }
    oled.setScale(1);
    if (set.shotVolume > 0) {
      oled.print(F(" мл"));
    }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_STALE_MENU) {
    printStr(F("ПИНАТЕЛЬ"), 1, 1, false);
    if (dosingGlassSelector == 0) {
      printStr(F("ТАЙМАУТ"), 3, 2, false);
    } else if (dosingGlassSelector == 1) {
      printStr(F("ПИНОК"), 3, 2, false);
    } else {
      printStr(T_BACK, 3, 2, true);
    }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_STALE_TIME) {
    if (dosingGlassSelector == 0) {
      printStr(F("ВРЕМЯ ОЖИДАНИЯ"), 1, 1, false);
      oled.setCursor(35, 3);
      oled.setScale(3);
      if (set.staleTimeS == 0) {
        oled.print(F("ВЫКЛ"));
      } else {
        oled.print(set.staleTimeS);
      }
      oled.setScale(1);
      if (set.staleTimeS > 0) {
        oled.print(F(" сек"));
      }
    } else if (dosingGlassSelector == 1) {
      printStr(F("ДЛИТЕЛЬНОСТЬ ПИНКА"), 1, 1, false);
      oled.setCursor(35, 3);
      oled.setScale(3);
      oled.print(set.staleAlertS);
      oled.setScale(1);
      oled.print(F(" сек"));
    }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 7) {
    printStr(F("ЯРКОСТЬ"), 1, 1, false);
    oled.setCursor(40, 3);
    oled.setScale(3);
    oled.print(set.ledBrightness);
    oled.setScale(1);
    oled.print(F(" кд"));
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 8) {
    printStr(F("ЗАДЕРЖКА ДАТЧИКОВ"), 1, 1, false);
    oled.setCursor(35, 4);
    oled.setScale(2);
    oled.print(set.sensorDelayMs);
    oled.setScale(1);
    oled.print(F(" мс"));
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_MODE) {
    if (toastSubItem == 0) {
      printStr(F("РЕЖИМ"), 1, 1, false);
      if (menuSelector == 0) {
        printStr(T_MANUAL, 3, 2, true);
      } else if (menuSelector == 1) {
        printStr(T_AUTO, 3, 2, true);
      } else if (menuSelector == 2) {
        printStr(T_ROULETTE, 3, 2, true);
      } else {
        printStr(T_BACK, 3, 2, true);
      }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStr(F("АКТИВИРОВАТЬ?"), 1, 1, false);
      if (dosingGlassSelector == 0) {
        printStr(T_ON, 3, 2, true);
      } else {
        printStr(T_OFF, 3, 2, true);
      }
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == SET_INVITE_MENU) {
    printStr(M3, 0, 1, true); // Пишем заголовок "ЗАЗЫВАЛА"
    if (toastSubItem == 0) {
      if (dosingGlassSelector == 0) printStr(S_T_OUT, 3, 1, false);
      else printStr(T_BACK, 3, 1, true);
    } else {
      oled.setCursor(35, 3);
      if (set.inviteTimeM == 0) {
        oled.setScale(3); oled.print(F("ВЫКЛ")); // Если 0 минут — пишем ВЫКЛ крупно
      } else {
        oled.setScale(3); oled.print(set.inviteTimeM);
        oled.setScale(1); printStr(S_MIN, 3, 1, true);
      }
    }
  }
  else if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      printStr(F("ТАМАДА"), 1, 1, false);
      if (dosingGlassSelector == 0) {
        printStr(M4, 3, 2, true);
      } else if (dosingGlassSelector == 1) {
        printStr(F("ПАУЗА"), 3, 2, false);
      } else if (dosingGlassSelector == 2) {
        printStr(F("ТАЙМАУТ"), 3, 2, false);
      } else if (dosingGlassSelector == 3) {
        printStr(F("ОТСРОЧКА"), 3, 2, false);
      } else {
        printStr(T_BACK, 3, 2, true);
      }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStr(F("ЗНАЧЕНИЕ:"), 1, 1, false);
      oled.setScale(3);
      oled.setCursor(45, 3);
      if (dosingGlassSelector == 0) {
        if (set.mp3Volume == 0) {
          oled.print(F("ВЫКЛ"));
        } else {
          oled.print(set.mp3Volume);
        }
      } else if (dosingGlassSelector == 1) {
        oled.print(set.toastPauseS);
        oled.setScale(1);
        oled.print(F(" сек"));
      } else if (dosingGlassSelector == 2) {
        oled.print(set.toastDelayS);
        oled.setScale(1);
        oled.print(F(" сек"));
      } else if (dosingGlassSelector == 3) {
        oled.print(set.startDelayS);
        oled.setScale(1);
        oled.print(F(" сек"));
      }
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == RESET_PAGE || state == PUMP_FLUSH) {
    if (state == RESET_PAGE) {
      printStr(M_RESET_CONF, 1, 1, true);
    } else {
      printStr(M_FLUSH_CONF, 1, 1, true);
    }

    if (dosingGlassSelector == 1) {
      if (state == RESET_PAGE) {
        printStr(T_RESET, 3, 2, true);
      } else {
        printStr(F("ПРОЛИВ"), 3, 2, false);
      }
    } else {
      printStr(T_BACK, 3, 2, true);
    }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == CAL_PUMP) {
    printStr(M_CALIB_TITLE, 1, 1, true);
    oled.setCursor(40, 4);
    oled.setScale(2);
    oled.print(set.mlTimeMs);
    oled.setScale(1);
    oled.print(F("мс"));
    printStr(F("Удержать Клик"), 7, 1, false);
  }
}
//--------------------------------------------------------------------------------------------------------- 9-я часть КОНЕЦ -------------------------------------------------------------------------------------------
