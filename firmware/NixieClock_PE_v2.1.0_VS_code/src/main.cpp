/*
  main.cpp — точка входа Arduino sketch, основной цикл и определения
  глобальных объектов/переменных.

  Файл унаследовал стандартный шаблон Arduino main.cpp (Arduino Team, 2005-2013, LGPL 2.1).
*/

#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

#include "effects.h"
#include "buttons.h"
#include "calculate_time.h"
#include "setup.h"
#include "set_time.h"
#include "DCDC.h"
#include "brightness.h"
#include "RTC_handler.h"
#include "global_externs.h"
#include "glitch.h"

// ==========================================================================
//  Периферия (определения; объявления — в peripherals_ext.h)
// ==========================================================================
RTC_DS3231 rtc;
Adafruit_BME280 bme;
Adafruit_Sensor *bme_temp     = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();
boolean isBMEhere;

GButton btnSet(BTN_NO_PIN, LOW_PULL, NORM_OPEN); // кнопка «М»
GButton btnL  (BTN_NO_PIN, LOW_PULL, NORM_OPEN); // кнопка «минус»
GButton btnR  (BTN_NO_PIN, LOW_PULL, NORM_OPEN); // кнопка «плюс»
GButton btnA  (ALARM_STOP, LOW_PULL, NORM_OPEN); // сенсорная кнопка

// ==========================================================================
//  Таймеры режимов
// ==========================================================================
byte flip_speed[] = {0, 60, 50, 40, 90, 90};
byte flip_effect_num = sizeof(flip_speed);

timerMinim flipTimer(flip_speed[flip_effect]); // таймер эффектов цифр
timerMinim glitchTimer(1000);                  // таймер «глюков»
timerMinim eshowTimer(300);                    // демонстрация номера эффекта
timerMinim autoTimer(ALARM_SH_TIME);           // автоматический выход из режимов
timerMinim measurementsTimer(MEASURE_PERIOD);  // обновление показаний BME
timerMinim autoShowMeasurementsTimer(0);       // автопоказ температуры/влажности/давления

// ==========================================================================
//  Состояние индикаторов
// ==========================================================================
volatile int8_t indiDimm[NUMTUB];   // величина диммирования (0-24)
volatile int8_t indiDigits[NUMTUB]; // отображаемые цифры (0-9)
byte newTime[NUMTUB];               // буфер новых значений для эффектов

byte anodeStates = 0x3F;            // битовая маска включённых разрядов
byte indiMaxBright = INDI_BRIGHT;
int  indiBrightCounter;

boolean newTimeFlag;
boolean newSecFlag;
boolean chBL = false;               // запрос обновления подсветки

// ==========================================================================
//  Время и синхронизация
// ==========================================================================
const unsigned int SQW_FREQ = 8192;    // ожидаемая частота SQW DS3231
volatile unsigned int SQW_counter = 0; // счётчик импульсов SQW
volatile boolean halfsecond = false;   // флаг прохождения полусекунды

int8_t hrs, mins, secs;
byte lastAdjustedMonth = 0;            // месяц последней автоподстройки (0 = не задано)

// ==========================================================================
//  Будильник
// ==========================================================================
int8_t alm_hrs, alm_mins;
boolean alm_set;
boolean alm_flag = ALARM_WAIT;

// ==========================================================================
//  Текущий режим и эффекты подсветки
// ==========================================================================
SH_MODES curMode = SHTIME;

byte backL_mode = 0;                // выбранный режим подсветки
byte backlColors[3] = {BACKLR, BACKLG, BACKLB};
byte backlColor;
boolean glitch_allowed = 1;
boolean auto_show_measurements = 1;
byte flip_effect = FM_SMOOTH;

static boolean showFlag = false;    // отображается номер эффекта при переходе

// ==========================================================================
//  Гамма-кривая для PWM (PROGMEM)
// ==========================================================================
static const uint8_t CRTgamma[256] PROGMEM = {
    0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   4,   4,
    4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,
    9,   9,   9,  10,  10,  10,  11,  11,  12,  12,  12,  13,  13,  14,  14,  15,
   15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,
   24,  25,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,  32,  33,  33,  34,
   35,  35,  36,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  47,
   48,  49,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
   63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  79,
   80,  81,  82,  83,  84,  85,  87,  88,  89,  90,  91,  93,  94,  95,  96,  98,
   99, 100, 101, 103, 104, 105, 107, 108, 109, 110, 112, 113, 115, 116, 117, 119,
  120, 121, 123, 124, 126, 127, 129, 130, 131, 133, 134, 136, 137, 139, 140, 142,
  143, 145, 146, 148, 149, 151, 153, 154, 156, 157, 159, 161, 162, 164, 165, 167,
  169, 170, 172, 174, 175, 177, 179, 180, 182, 184, 186, 187, 189, 191, 193, 194,
  196, 198, 200, 202, 203, 205, 207, 209, 211, 213, 214, 216, 218, 220, 222, 224,
  226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
};

byte getPWM_CRT(byte val)
{
  return pgm_read_byte(&(CRTgamma[val]));
}

// ==========================================================================
//  Низкоуровневое управление пинами и аппаратным PWM
// ==========================================================================

/* Быстрое управление выходными пинами (аналог digitalWrite).
 * Принудительно отключает PWM на пинах с аппаратной поддержкой.
 */
void setPin(byte pin, byte x)
{
  switch (pin)
  { // отключить PWM (Compare-Output-Mode бит) на пине
  case 3:  bitClear(TCCR2A, COM2B1); break;
  case 5:  bitClear(TCCR0A, COM0B1); break;
  case 6:  bitClear(TCCR0A, COM0A1); break;
  case 9:  bitClear(TCCR1A, COM1A1); break;
  case 10: bitClear(TCCR1A, COM1B1); break;
  case 11: bitClear(TCCR2A, COM2A1); break;
  }

  x = (x != 0);
  if      (pin < 8)  bitWrite(PORTD, pin,      x);
  else if (pin < 14) bitWrite(PORTB, pin -  8, x);
  else if (pin < 20) bitWrite(PORTC, pin - 14, x);
}

/* Быстрое управление аппаратным PWM (аналог analogWrite).
 * Работает только на пинах с аппаратной поддержкой.
 */
void setPWM(byte pin, byte duty)
{
  if (duty == 0) { setPin(pin, LOW); return; }
  switch (pin)
  {
  case 3:  bitSet(TCCR2A, COM2B1); OCR2B = duty; break;
  case 5:  bitSet(TCCR0A, COM0B1); OCR0B = duty; break;
  case 6:  bitSet(TCCR0A, COM0A1); OCR0A = duty; break;
  case 9:  bitSet(TCCR1A, COM1A1); OCR1A = duty; break;
  case 10: bitSet(TCCR1A, COM1B1); OCR1B = duty; break;
  case 11: bitSet(TCCR2A, COM2A1); OCR2A = duty; break;
  }
}

// ==========================================================================
//  Стандартные функции Arduino main.cpp (weak)
// ==========================================================================
int atexit(void (* /*func*/)()) { return 0; }
void initVariant() __attribute__((weak));
void initVariant() {}
void setupUSB() __attribute__((weak));
void setupUSB() {}

// ==========================================================================
//  Основной цикл
// ==========================================================================
void loop()
{
  // полусекундный тик — ход времени и пересчёт зависящих от него величин
  if (halfsecond)
  {
    if (startup_delay) startup_delay--;
    calculateTime(&dotBrightFlag, &dotBrightDirection, &dotBrightCounter);
  }

  beeper();

  // пока показывается номер эффекта — приостанавливаем перелистывание цифр
  if (showFlag)
  {
    if (eshowTimer.isReady()) showFlag = false;
  }
  else if ((newSecFlag || newTimeFlag) && curMode == SHTIME)
  {
    flipTick();
  }

  dotBrightTick();
  backlBrightTick();
  if (glitch_allowed && curMode == SHTIME) glitchTick();
  buttonsTick(&showFlag, &SQW_counter, &chBL);
  DCDCTick();
}

int main(void)
{
  init();
  initVariant();
#if defined(USBCON)
  USBDevice.attach();
#endif
  setup();
  for (;;)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }
  return 0;
}
