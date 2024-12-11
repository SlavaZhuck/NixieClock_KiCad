/*
  main.cpp - Main loop for Arduino sketches
  Copyright (c) 2005-2013 Arduino Team.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "NixieClock_PE_v2.1.0.h"
// #include <Arduino.h>
//  библиотеки
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
#include "beeper.h"
#include "global_externs.h"
#include "glitch.h"

// периферия
RTC_DS3231 rtc;      // создаём класс управления DS3231 (используем интерфейс I2C)
Adafruit_BME280 bme; // создаём класс управления BME (используем интерфейс I2C)
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

byte flip_speed[] = {0, 60, 50, 40, 90, 90};
byte flip_effect = FM_SMOOTH; // Выбранный активен при первом запуске и меняется кнопками.

// таймеры

timerMinim flipTimer(flip_speed[flip_effect]); // таймер эффектов
timerMinim glitchTimer(1000);                  // таймер глюков


timerMinim eshowTimer(300); // таймер демонстрации номера эффекта

timerMinim autoTimer(ALARM_SH_TIME);          // таймер автоматического выхода из режимов
timerMinim measurementsTimer(MEASURE_PERIOD); // таймер обновления показаний
timerMinim autoShowMeasurementsTimer(0);      // таймер автопоказа темпера...

// кнопки
GButton btnSet(BTN_NO_PIN, LOW_PULL, NORM_OPEN); // инициализируем кнопку Set ("М")
GButton btnL(BTN_NO_PIN, LOW_PULL, NORM_OPEN);   // инициализируем кнопку Down (Left) ("минус")
GButton btnR(BTN_NO_PIN, LOW_PULL, NORM_OPEN);   // инициализируем кнопку Up (Right) ("плюс")
GButton btnA(ALARM_STOP, LOW_PULL, NORM_OPEN);   // инициализируем кнопку Alarm ("сенсор")
// переменные
volatile int8_t indiDimm[NUMTUB];    // величина диммирования (0-24)

volatile int8_t indiDigits[NUMTUB];  // цифры, которые должны показать индикаторы (0-10)


// синхронизация
const unsigned int SQW_FREQ = 8192;    // частота SQW сигнала
volatile unsigned int SQW_counter = 0; // новый таймер
volatile boolean halfsecond = false;   // полсекундный таймер



volatile unsigned int note_num = 0;   // номер ноты в мелодии
volatile unsigned int note_count = 0; // фаза сигнала
volatile boolean note_up_low = false; // true - высокий уровень, false - низкий уровень
volatile boolean note_ip = false;     // true - сигнал звучит, false - сигнал не звучит


int8_t hrs, mins, secs;        // часы, минуты, секунды
int8_t alm_hrs, alm_mins;      // сохранение времени будильника
boolean alm_set;               // будильник установлен? (берётся из памяти)
boolean alm_flag = ALARM_WAIT; // будильник сработал?

/* всё про подсветку */
byte backlColors[3] = {BACKLR, BACKLG, BACKLB};
byte backlColor;

/* всё про bme280 */
boolean isBMEhere;

boolean chBL = false; // для обнаружения необходимости смены подсветки

/* переменнные из исходного скетча */
byte indiMaxBright = INDI_BRIGHT;

int indiBrightCounter;
boolean newTimeFlag;
boolean newSecFlag; // добавлен для исключения секунд из части эффектов
byte newTime[NUMTUB];

/* дополнительные переменные */
boolean showFlag = false; // признак демонстрации номера эффекта
SH_MODES curMode = SHTIME;
boolean currentDigit = false;


byte anodeStates = 0x3F; // в оригинальном скетче было массивом логических переменных
                         // заменено на байт, биты (начиная с 0), которого определяют
                         // необходимость включения разрядов (начиная со старшего)


// *********************** ДЛЯ РАЗРАБОТЧИКОВ ***********************
byte backL_mode = 0; // Выбранный режим активен при запуске и меняется кнопками
                     // скорость эффектов, мс (количество не меняй)
                     // количество эффектов
byte flip_effect_num = sizeof(flip_speed);
boolean glitch_allowed = 1;         // 1 - включить, 0 - выключить глюки. Управляется кнопкой
boolean auto_show_measurements = 1; // автоматически показывать измерения Temp, Bar, Hum



static const uint8_t CRTgamma[256] PROGMEM = {
  0,    0,    1,    1,    1,    1,    1,    1,
  1,    1,    1,    1,    1,    1,    1,    1,
  2,    2,    2,    2,    2,    2,    2,    2,
  3,    3,    3,    3,    3,    3,    4,    4,
  4,    4,    4,    5,    5,    5,    5,    6,
  6,    6,    7,    7,    7,    8,    8,    8,
  9,    9,    9,    10,   10,   10,   11,   11,
  12,   12,   12,   13,   13,   14,   14,   15,
  15,   16,   16,   17,   17,   18,   18,   19,
  19,   20,   20,   21,   22,   22,   23,   23,
  24,   25,   25,   26,   26,   27,   28,   28,
  29,   30,   30,   31,   32,   33,   33,   34,
  35,   35,   36,   37,   38,   39,   39,   40,
  41,   42,   43,   43,   44,   45,   46,   47,
  48,   49,   49,   50,   51,   52,   53,   54,
  55,   56,   57,   58,   59,   60,   61,   62,
  63,   64,   65,   66,   67,   68,   69,   70,
  71,   72,   73,   74,   75,   76,   77,   79,
  80,   81,   82,   83,   84,   85,   87,   88,
  89,   90,   91,   93,   94,   95,   96,   98,
  99,   100,  101,  103,  104,  105,  107,  108,
  109,  110,  112,  113,  115,  116,  117,  119,
  120,  121,  123,  124,  126,  127,  129,  130,
  131,  133,  134,  136,  137,  139,  140,  142,
  143,  145,  146,  148,  149,  151,  153,  154,
  156,  157,  159,  161,  162,  164,  165,  167,
  169,  170,  172,  174,  175,  177,  179,  180,
  182,  184,  186,  187,  189,  191,  193,  194,
  196,  198,  200,  202,  203,  205,  207,  209,
  211,  213,  214,  216,  218,  220,  222,  224,
  226,  228,  230,  232,  233,  235,  237,  239,
  241,  243,  245,  247,  249,  251,  253,  255,
};

/* Гамма-характеристика светимости из программной памяти
 *  Входные параметры:
 *    byte val: абсолютное значение светимости
 *  Выходные параметры:
 *    byte: скорректированное значение светимости в соответствии с кривой гамма
 */
byte getPWM_CRT(byte val)
{
  return pgm_read_byte(&(CRTgamma[val]));
}

/* Быстрое управление выходными пинами Ардуино (аналог digitalWrite)
 *  Входные параметры:
 *    byte pin: номер пина в нотации Ардуино, подлежащий изменению
 *    byte х: значение, в которое устанавливается pin (0 или 1)
 *  Выходные параметры: нет
 */
void setPin(byte pin, byte x)
{
  switch (pin)
  {       // откл pwm
  case 3: // 2B
    bitClear(TCCR2A, COM2B1);
    break;
  case 5: // 0B
    bitClear(TCCR0A, COM0B1);
    break;
  case 6: // 0A
    bitClear(TCCR0A, COM0A1);
    break;
  case 9: // 1A
    bitClear(TCCR1A, COM1A1);
    break;
  case 10: // 1B
    bitClear(TCCR1A, COM1B1);
    break;
  case 11: // 2A
    bitClear(TCCR2A, COM2A1);
    break;
  }

  x = ((x != 0) ? 1 : 0);
  if (pin < 8)
    bitWrite(PORTD, pin, x);
  else if (pin < 14)
    bitWrite(PORTB, (pin - 8), x);
  else if (pin < 20)
    bitWrite(PORTC, (pin - 14), x);
  else
    return;
}

/* Быстрое управление ШИМ Ардуино (аналог analogWrite). Работает только на пинах с аппаратной поддержкой.
 *  Входные параметры:
 *    byte pin: номер пина в нотации Ардуино, подлежащий изменению
 *    byte duty: относительное значение длительности импульса на выбранном pin
 *  Выходные параметры: нет
 */
void setPWM(byte pin, byte duty)
{
  if (duty == 0)
    setPin(pin, LOW);
  else
  {
    switch (pin)
    {
    case 5:
      bitSet(TCCR0A, COM0B1);
      OCR0B = duty;
      break;
    case 6:
      bitSet(TCCR0A, COM0A1);
      OCR0A = duty;
      break;
    case 10:
      bitSet(TCCR1A, COM1B1);
      OCR1B = duty;
      break;
    case 9:
      bitSet(TCCR1A, COM1A1);
      OCR1A = duty;
      break;
    case 3:
      bitSet(TCCR2A, COM2B1);
      OCR2B = duty;
      break;
    case 11:
      bitSet(TCCR2A, COM2A1);
      OCR2A = duty;
      break;
    default:
      break;
    }
  }
}


// Declared weak in Arduino.h to allow user redefinitions.
int atexit(void (* /*func*/)()) { return 0; }

// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() {}

void setupUSB() __attribute__((weak));
void setupUSB() {}








/* check 28.10.20 */
/*
 */

void loop()
{

  if (halfsecond)
    calculateTime(); // каждые 500 мс пересчёт и отправка времени
  beeper();
  if (showFlag)
  { // отображение номера эффекта для цифр при переходе
    if (eshowTimer.isReady())
      showFlag = false;
  }
  else
  {
    if ((newSecFlag || newTimeFlag) && curMode == 0)
    {
      flipTick(); // перелистывание цифр
    }
  }
  dotBrightTick();   // плавное мигание точки
  backlBrightTick(); // плавное мигание подсветки ламп
  if (glitch_allowed && curMode == SHTIME)
    glitchTick(); // глюки
  buttonsTick();  // кнопки
  DCDCTick();     // анодное напряжение
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
    if (serialEventRun)
      serialEventRun();
  }

  return 0;
}
