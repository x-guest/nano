#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// ⚙️ ЖЕСТКАЯ КАРТА РАСКЛЮЧЕНИЯ ПИНОВ ПЕРИФЕРИИ НА ПЛАТЕ ARDUINO NANO
// ============================================================================
#define NUM_GLASSES 6           // Количество рюмок в системе (строго от 1 до 6)
#define LED_PIN 13              // Выход управления адресной LED-лентой WS2812B (D13)

// Пины органов управления (Энкодер KY-040)
#define ENCODER_CLK 2           // Шаг вращения А (Аппаратное прерывание D2)
#define ENCODER_DT 3            // Шаг вращения В (Аппаратное прерывание D3)
#define ENCODER_SW 17           // Кнопка клика переехала на аналоговую ногу А3 (цифровой D17)

// Пины исполнительных устройств и концевиков механики
#define PUMP_PIN 4              // Выход управления голым транзистором IRF520N помпы (D4)
#define HOME_SW_PIN 12          // Вход микроконцевика KW10 нулевой точки башни "ДОМ" (D12)

// Пины аппаратного подключения MP3-плеера DFPlayer Mini (UART на пинах D0 и D1)
#define MP3_TX_PIN 0            
#define MP3_RX_PIN 1            
#define MP3_BUSY_PIN 11         

// Пины силового драйвера шагового мотора ULN2003
#define STEPPER_IN1 7           
#define STEPPER_IN2 8           
#define STEPPER_IN3 9           
#define STEPPER_IN4 10          

const int pinSensors[NUM_GLASSES] = { 14, 15, 16, 5, A6, A7 };

// ============================================================================
// 🎯 ЗАВОДСКИЕ ПАРАМЕТРЫ СИСТЕМЫ ПО УМОЛЧАНИЮ
// ============================================================================
const int VOLUME_STEP_ML = 5;             // Шаг изменения объема налива (мл)
const int DEFAULT_SHOT_VOLUME = 30;       // Базовая порция налива (мл)
const int DEFAULT_ML_TIME_MS  = 2500;     // Время работы насоса на 50 мл (мс) - КАЛИБРУЕТСЯ
const int DEFAULT_SAVED_MODE  = 0;        // Стартовый режим: 0-Ручной, 1-Авто, 2-Рулетка
const int DEFAULT_TOAST_DELAY = 15;       
const int DEFAULT_TOAST_PAUSE = 1;        
const int DEFAULT_VOLUME = 20;            

const int DEFAULT_LED_BRIGHTNESS = 100;   // Общая максимальная яркость системы (10-255)
const int DEFAULT_ROULETTE_SPEED = 5;     
const int DEFAULT_SENSOR_DELAY_MS = 800;  

const int MIN_POUR_VOLUME = 0;            // Нижний порог 0 = рюмка ВЫКЛЮЧЕНА
const int MAX_POUR_VOLUME = 150;          
const int SLEEP_TIMEOUT_MINUTES = 5;      

// 🎯 Параметры функции "Пинатель" (Напоминание о полном стакане)
const int DEFAULT_STALE_TIME_S = 30;      // Заводское время простоя рюмки до напоминания (сек)
const int STALE_TIME_STEP_S = 5;          // Шаг изменения времени простоя в меню (сек)
const int MAX_STALE_TIME_S = 180;         // Максимальное настраиваемое время простоя (сек)
const int STALE_SOUND_TRACK_ID = 3;       // Номер трека напоминания ("003.mp3" в системной папке "00")

// Настройки длительности самого оповещения
const int DEFAULT_STALE_ALERT_S = 5;      // Заводская длительность пинка (сек)
const int STALE_ALERT_STEP_S = 1;         // Шаг изменения длительности пинка в меню (сек)
const int MAX_STALE_ALERT_S = 30;         // Максимальная длительность пинка в меню (сек)

// 🎨 КАСТОМИЗАЦИЯ ПОДСВЕТКИ И ЯРКОСТИ РЮМОК
#define COLOR_STANDBY   CRGB(200, 200, 200) // Цвет дежурного режима (Рюмок нет)
#define COLOR_ACTIVE    CRGB(255, 255, 255) // Цвет активной рюмки на столе (>0 мл)
#define COLOR_DISABLED  CRGB(255, 0, 0)     // Цвет выключенной рюмки на столе (0 мл)
#define COLOR_POURED    CRGB(0, 255, 0)     // Цвет наполненной рюмки (Зеленый)

const int BRIGHT_STANDBY  = 15;   // Яркость дежурного режима (тусклый белый)
const int BRIGHT_ACTIVE   = 180;  // Яркость активной рюмки (яркий белый)
const int BRIGHT_DISABLED = 25;   // Яркость отключенной рюмки (тусклый красный)
const int BRIGHT_POURED   = 150;  // Яркость налитой рюмки (зеленый)

// Параметры скорости шагового двигателя
const int MOTOR_MAX_SPEED_POUR = 800;     
const int MOTOR_MAX_SPEED_CALIB = 400;    
const int MOTOR_ACCELERATION_RATE = 1200; 
const int CALIBRATION_STEP_STEPS = 100;         

// Настройка папок на MicroSD-карте
const int SYSTEM_SOUND_FOLDER = 0;        
const int ROULETTE_TRACK_ID = 1;          
const int TOAST_SOUND_FOLDER = 1;         
const int TOTAL_TOAST_TRACKS = 100;       

const int DEFAULT_ROULETTE_SPIN_S = 4;    
const int MIN_ROULETTE_SPIN_S = 4;        
const int MAX_ROULETTE_SPIN_S = 15;       

const int DOUBLE_CLICK_TIMEOUT_MS = 350;  
const int DEFAULT_SHOT_POS[NUM_GLASSES] = { 250, 590, 930, 1270, 1610, 1950 };
const int DEFAULT_HOME_OFFSET = 0;        
const int stepsPerRevolution = 4096;      

const int TOTAL_MENU_ITEMS = 11;          // [ИСПРАВЛЕНО]: В корне меню снова строго 11 физических пунктов, пустых мест нет
#define HISTORY_SIZE 8                    

struct Settings {
  int shotVolume;                         
  int mlTimeMs;                           
  int shotPos[NUM_GLASSES];               
  int homePos;                            
  int savedMode;                          
  int toastDelayS;                        
  int toastPauseS;                        
  int shotVolumeIndividual[NUM_GLASSES];  
  bool shotVolumeCustomized[NUM_GLASSES]; 
  int mp3Volume;                          
  int ledBrightness;                      
  int rouletteSpeed;                      
  int sensorDelayMs;                      
  int rouletteSpinS;                      
  int startDelayS;                        
  int staleTimeS;                         
  int staleAlertS;                        
};

#endif
