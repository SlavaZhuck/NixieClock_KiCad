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
//#include <Arduino.h>
// библиотеки
#include "timer2Minim.h"
#include <GyverButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <EEPROM.h>

// периферия
RTC_DS3231 rtc;                                   // создаём класс управления DS3231 (используем интерфейс I2C)
Adafruit_BME280 bme;                              // создаём класс управления BME (используем интерфейс I2C)
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

// таймеры
timerMinim dotBrightTimer(DOT_TIMER);             // таймер шага яркости точки
timerMinim backlBrightTimer(30);                  // таймер шага яркости подсветки
timerMinim almTimer((long)ALM_TIMEOUT * 1000);    // таймер времени звучания будильника
timerMinim flipTimer(FLIP_SPEED[FLIP_EFFECT]);    // таймер эффектов
timerMinim glitchTimer(1000);                     // таймер глюков
timerMinim blinkTimer(500);                       // таймер моргания
#define L16   127                                 // длительность 1/16 в мс при темпе 118 четвёртых в минуту
timerMinim noteTimer(L16);                        // таймер длительности ноты
timerMinim eshowTimer(300);                       // таймер демонстрации номера эффекта
#define ALARM_SH_TIME                 5000        // время отображения установленного будильника
#define TEMP_SH_TIME                  2000        // время показа температуры до перехода к влажности
#define HUMIDITY_SH_TIME              2000        // время показа влажности до перехода к давлению
#define ATMOSPHERE_SH_TIME            2000        // время показа давления до возврата к отображению времени
#define MEASURE_PERIOD                1000        // период обновления показаний

timerMinim autoTimer(ALARM_SH_TIME);              // таймер автоматического выхода из режимов
timerMinim measurementsTimer(MEASURE_PERIOD);     // таймер обновления показаний
timerMinim autoShowMeasurementsTimer(0);      // таймер автопоказа темпера...

// кнопки
GButton btnSet(BTN_NO_PIN, LOW_PULL, NORM_OPEN);  // инициализируем кнопку Set ("М")
GButton btnL(BTN_NO_PIN, LOW_PULL, NORM_OPEN);    // инициализируем кнопку Down (Left) ("минус")
GButton btnR(BTN_NO_PIN, LOW_PULL, NORM_OPEN);    // инициализируем кнопку Up (Right) ("плюс")
GButton btnA(ALARM_STOP, LOW_PULL, NORM_OPEN);    // инициализируем кнопку Alarm ("сенсор")

// переменные
volatile int8_t indiDimm[NUMTUB];                 // величина диммирования (0-24)
volatile int8_t indiCounter[NUMTUB];              // счётчик каждого индикатора (0-24)
volatile int8_t indiDigits[NUMTUB];               // цифры, которые должны показать индикаторы (0-10)
volatile int8_t curIndi;                          // текущий индикатор (0-5)

// синхронизация
const unsigned int SQW_FREQ = 8192;               // частота SQW сигнала
volatile unsigned int SQW_counter = 0;            // новый таймер
volatile boolean halfsecond = false;              // полсекундный таймер 

/* мелодия */
                                                  // мелодия (длительность импульса, длительность паузы - в циклах таймера, длительность ноты - в мс)
const uint8_t NotePrescalerHigh[] = { 25,        0,    25,    21,    25,    28,    31,    33,    25,     0,    25,    21,    25,    28,    31,    28,    31,    33 };
const uint8_t NotePrescalerLow[] =  { 25,      255,    25,    21,    25,    28,    32,    33,    25,   255,    25,    21,    25,    28,    32,    28,    32,    33 };
const uint16_t NoteLength[] =       { L16*3, L16*3, L16*2, L16*3, L16*3, L16*2, L16*8, L16*8, L16*3, L16*3, L16*2, L16*3, L16*3, L16*2, L16*3, L16*3, L16*2, L16*8 };
                                                  // длительность мелодии в нотах
const uint8_t notecounter = sizeof(NotePrescalerHigh);
volatile unsigned int note_num = 0;               // номер ноты в мелодии
volatile unsigned int note_count = 0;             // фаза сигнала
volatile boolean note_up_low = false;             // true - высокий уровень, false - низкий уровень
volatile boolean note_ip = false;                 // true - сигнал звучит, false - сигнал не звучит

/* регулировка напряжения */
const uint8_t iduty = 10;                         // период интегрирования ошибки напряжения
uint8_t idcounter = 0;                            // текущий период интегрирования
const int8_t maxerrduty = 10;                     // максимальное значение интегрированной ошибки напряжения
int duty_delta = 0;                               // текущая интегральная ошибка
const int nominallevel = 490;                     // номинальное значение напряжения
const uint8_t maxduty = 220;                      // защита от чрезмерного повышения напряжения
const uint8_t minduty = 10;                       // защита от выключения
uint8_t r_duty;                                   // актуальная скважность ШИМ анодного напряжения
int8_t startup_delay = 5;                         // задержка в применении нового значения ШИМ после запуска

/* макроопределения битовых операций */
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit)) 
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))


volatile boolean dotFlag = false;                 // признак фазы внутри половины секунды

int8_t hrs, mins, secs;                           // часы, минуты, секунды

/* всё про будильник */
#define ALARM_RESET     false                     // будильник не установлен
#define ALARM_SET       true                      // будильник установлен

#define ALARM_WAIT      false                     // будильник не сработал
#define ALARM_FIRED     true                      // будильник сработал

#define ALARM_WAIT_1    false                     // будильник ещё не сработал
#define ALARM_IN_MIN    true                      // будильник сработал меньше минуты назад

#define ALARM_NOREQ     false                     // запроса на проверку будильника нет
#define ALARM_REQ       true                      // запрос на проверку будильника

int8_t alm_hrs, alm_mins;                         // сохранение времени будильника
boolean alm_set;                                  // будильник установлен? (берётся из памяти)
boolean alm_flag = ALARM_WAIT;                    // будильник сработал?
boolean alm_fired = ALARM_WAIT_1;                 // запрет повторного срабатывания будильника в ту же минуту
boolean alm_request = ALARM_NOREQ;                // признак необходимости проверить совпадение времени с будильником

/* всё про подсветку */
byte backlColors[3] = { BACKLR, BACKLG, BACKLB };
byte backlColor;

/* всё про точку */
DOT_MODES dotMode;                                // текущий установленный режим работы точки
boolean dotBrightFlag, dotBrightDirection;        // индикатор времени начала отображения точки, точка по яркости возрастает/уменьшается
byte dotMaxBright = DOT_BRIGHT;                   // максимальная яркость точки
int dotBrightCounter;                             // текущая яркость точки в процессе эффекта
byte dotBrightStep;                               // шаг изменения яркости точки в нормальных условиях
byte dotNumBlink;                                 // количество включений точки за период

/* всё про bme280 */
sensors_event_t temp_event, pressure_event, humidity_event;
boolean isBMEhere;
boolean isFreeze = false;

boolean chBL = false;                             // для обнаружения необходимости смены подсветки

/* переменнные из исходного скетча */
boolean changeFlag;
boolean blinkFlag;
byte indiMaxBright = INDI_BRIGHT, backlMaxBright = BACKL_BRIGHT;
boolean backlBrightFlag, backlBrightDirection, indiBrightDirection;
int backlBrightCounter, indiBrightCounter;
boolean newTimeFlag;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
boolean newSecFlag;                               // добавлен для исключения секунд из части эффектов
#endif
boolean flipIndics[NUMTUB];
byte newTime[NUMTUB];
boolean flipInit;
byte startCathode[NUMTUB], endCathode[NUMTUB];
byte glitchCounter, glitchMax, glitchIndic;
boolean glitchFlag, indiState;

/* дополнительные переменные */
boolean showFlag = false;                         // признак демонстрации номера эффекта

enum SH_MODES: byte
                   { SHTIME,                      // (0) отображение часов
                     SETTIME,                     // (1) установка часов
                     SHALARM,                     // (2) отображение времени будильника (5 сек)
                     SETALARM,                    // (3) установка времени будильника
                     SHTEMP,                      // (4) отображение температуры (5 сек)
                     SHHUM,                       // (5) отображение влажности (5 сек)
                     SHATM};                      // (6) отображение атмосферного давления (5 сек)
SH_MODES curMode = SHTIME;

enum SAVE_PARAMS: byte
                   { FLIPEFF,                     // (0) эффект для цифр
                     LIGHTEFF,                    // (1) эффект для подсветки
                     GLEFF,                       // (2) эффект глюков
                     VSET,                        // (3) установка стабилизатора напряжения (не используется)
                     ALHOUR,                      // (4) сохранение времени будильника/часы
                     ALMIN,                       // (5) сохранение времени будильника/минуты
                     ALIFSET,                     // (6) сохранение статуса будильника (заведён/сброшен)
                     BLCOLOR,                     // (7) текущий цвет подсветки
                     AUTOSHOWMEAS};               // (8) автоматически показывать измерения
                     
boolean currentDigit = false;
int8_t changeHrs, changeMins;
boolean lampState = false;
#if (NUMTUB == 6)
byte anodeStates = 0x3F;                          // в оригинальном скетче было массивом логических переменных
                                                  // заменено на байт, биты (начиная с 0), которого определяют
                                                  // необходимость включения разрядов (начиная со старшего)
#else
byte anodeStates = 0x0F;                          // в оригинальном скетче было массивом логических переменных
                                                  // заменено на байт, биты (начиная с 0), которого определяют
                                                  // необходимость включения разрядов (начиная со старшего)

#endif
byte currentLamp, flipEffectStages;
bool trainLeaving;

const uint8_t CRTgamma[256] PROGMEM = {
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

void RTC_handler(void);
//my variables
uint16_t timestamp_half_sec = 0;

/* Гамма-характеристика светимости из программной памяти
 *  Входные параметры:
 *    byte val: абсолютное значение светимости
 *  Выходные параметры:
 *    byte: скорректированное значение светимости в соответствии с кривой гамма
 */
byte getPWM_CRT(byte val) {
  return pgm_read_byte(&(CRTgamma[val]));
}

/* Быстрое управление выходными пинами Ардуино (аналог digitalWrite)
 *  Входные параметры:
 *    byte pin: номер пина в нотации Ардуино, подлежащий изменению
 *    byte х: значение, в которое устанавливается pin (0 или 1)
 *  Выходные параметры: нет
 */
void setPin(byte pin, byte x) {
  switch (pin) {                                  // откл pwm
    case 3:                                       // 2B
      bitClear(TCCR2A, COM2B1);
      break;
    case 5:                                       // 0B
      bitClear(TCCR0A, COM0B1);
      break;
    case 6:                                       // 0A
      bitClear(TCCR0A, COM0A1);
      break;
    case 9:                                       // 1A
      bitClear(TCCR1A, COM1A1);
      break;
    case 10:                                      // 1B
      bitClear(TCCR1A, COM1B1);
      break;
    case 11:                                      // 2A
      bitClear(TCCR2A, COM2A1);
      break;
  }

  x = ((x != 0) ? 1 : 0);
  if (pin < 8) bitWrite(PORTD, pin, x);
  else if (pin < 14) bitWrite(PORTB, (pin - 8), x);
  else if (pin < 20) bitWrite(PORTC, (pin - 14), x);
  else return;
}


/* Быстрое управление ШИМ Ардуино (аналог analogWrite). Работает только на пинах с аппаратной поддержкой.
 *  Входные параметры:
 *    byte pin: номер пина в нотации Ардуино, подлежащий изменению
 *    byte duty: относительное значение длительности импульса на выбранном pin
 *  Выходные параметры: нет
 */
void setPWM(byte pin, byte duty) {
  if (duty == 0) setPin(pin, LOW);
  else {
    switch (pin) {
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
int atexit(void (* /*func*/ )()) { return 0; }

// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() { }

void setupUSB() __attribute__((weak));
void setupUSB() { }

/* check 28.10.20 
 *  
*/

/* Заполнение массива отображаемых данных
 *  Входные параметры:
 *    byte hours: двузначное число, отображаемое в разрядах часов;
 *    byte minutes: двузначное число, отображаемое в разрядах минут;
 *    byte seconds: двузначное число, отображаемое в разрядах секунд.
 *  Выходные параметры: нет
 */
void sendTime(byte hours, byte minutes, byte seconds) 
{
  indiDigits[0] = (byte)hours / 10;
  indiDigits[1] = (byte)hours % 10;

  indiDigits[2] = (byte)minutes / 10;
  indiDigits[3] = (byte)minutes % 10;

  indiDigits[4] = (byte)seconds / 10;
  indiDigits[5] = (byte)seconds % 10;
}


/* Заполнение массива новых данных для работы эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void setNewTime() 
{
  newTime[0] = (byte)hrs / 10;
  newTime[1] = (byte)hrs % 10;

  newTime[2] = (byte)mins / 10;
  newTime[3] = (byte)mins % 10;
  newTime[4] = (byte)secs / 10;
  newTime[5] = (byte)secs % 10;  
}

/* Обеспечение смены яркости от времени суток
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void changeBright() 
{
#if (NIGHT_LIGHT == 1)
  // установка яркости всех светилок от времени суток
  if ( (hrs >= NIGHT_START && hrs <= 23)
       || (hrs >= 0 && hrs < NIGHT_END) ) {
    indiMaxBright = INDI_BRIGHT_N;
    dotMaxBright = DOT_BRIGHT_N;
    backlMaxBright = BACKL_BRIGHT_N;
  } else {
    indiMaxBright = INDI_BRIGHT;
    dotMaxBright = DOT_BRIGHT;
    backlMaxBright = BACKL_BRIGHT;
  }
#else
  indiMaxBright = INDI_BRIGHT;
  dotMaxBright = DOT_BRIGHT;
  backlMaxBright = BACKL_BRIGHT;
#endif

  memset((void*)indiDimm, indiMaxBright, NUMTUB);

  // установить режим работы секундной точки в зависимости от установок будильника
  dotSetMode( alm_set ? DOT_IN_ALARM : DOT_IN_TIME );

  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
  indiBrightCounter = indiMaxBright;

  //change PWM to apply backlMaxBright in case of maximum bright mode
  if (BACKL_MODE == 1) 
	  setPWM(backlColors[backlColor], backlMaxBright);
}


/* Структурная функция начальной установки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void setup() {
  // случайное зерно для генератора случайных чисел
  randomSeed(analogRead(6) + analogRead(7));

  // настройка пинов на вход
  pinMode(ALARM_STOP, INPUT);

  // настройка пинов на выход
  pinMode(DECODER0, OUTPUT);
  pinMode(DECODER1, OUTPUT);
  pinMode(DECODER2, OUTPUT);
  pinMode(DECODER3, OUTPUT);
  pinMode(KEY0, OUTPUT);
  pinMode(KEY1, OUTPUT);
  pinMode(KEY2, OUTPUT);
  pinMode(KEY3, OUTPUT);

  pinMode(KEY4, OUTPUT);
  pinMode(KEY5, OUTPUT);

  pinMode(PIEZO, OUTPUT);
  pinMode(GEN, OUTPUT);
  pinMode(DOT, OUTPUT);
  pinMode(BACKLR, OUTPUT);
  pinMode(BACKLG, OUTPUT);
  pinMode(BACKLB, OUTPUT);
  
  digitalWrite(GEN, 0);                           // устранение возможного "залипания" выхода генератора
  btnSet.setTimeout(400);                         // установка параметров библиотеки реагирования на кнопки
  btnSet.setDebounce(90);                         // защитный период от дребезга
  btnR.setDebounce(90);                           // защитный период от дребезга
  btnL.setDebounce(90);                           // защитный период от дребезга

 // ---------- RTC -----------
  rtc.begin();
  if (rtc.lostPower()) 
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  pinMode(RTC_SYNC, INPUT_PULLUP);                // объявляем вход для синхросигнала RTC
                                                  // заставляем входной сигнал генерировать прерывания
  attachInterrupt(digitalPinToInterrupt(RTC_SYNC), RTC_handler, RISING);
  rtc.writeSqwPinMode(DS3231_SquareWave8kHz);     // настраиваем DS3231 для вывода сигнала 8кГц

  // настройка быстрого чтения аналогового порта (mode 4)
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
  analogRead(A6);                                 // устранение шума
  analogRead(A7);
  // ------------------
  boolean time_sync = false;
  DateTime now = rtc.now();
  do 
  {
    if (!time_sync) 
    {
      time_sync = true;
      secs = now.second();
      mins = now.minute();
      hrs = now.hour();
    }
    now = rtc.now();
  } while (secs != now.second());
  secs = now.second();
  SQW_counter = 0;
  mins = now.minute();
  hrs = now.hour();
      
  // задаем частоту ШИМ на 9 и 10 выводах 31 кГц
  TCCR1B = (TCCR1B & 0b11111000) | 1;               // ставим делитель 1

  // перенастраиваем частоту ШИМ на пинах 3 и 11 для соответствия таймеру 0
  // Пины D3 и D11 - 980 Гц
  TCCR2B = 0b00000011;                            // x32
  TCCR2A = 0b00000001;                            // phase correct

  // EEPROM
  if (EEPROM.read(1023) != 103) {                 // первый запуск
    EEPROM.put(1023, 103);
    EEPROM.put(FLIPEFF, FLIP_EFFECT);
    EEPROM.put(LIGHTEFF, BACKL_MODE);
    EEPROM.put(GLEFF, GLITCH_ALLOWED);
    EEPROM.put(ALHOUR, 0);
    EEPROM.put(ALMIN, 0);
    EEPROM.put(ALIFSET, false);
    EEPROM.put(BLCOLOR, 1);
    EEPROM.put(AUTOSHOWMEAS, 1);
  }
  EEPROM.get(FLIPEFF, FLIP_EFFECT);
  EEPROM.get(LIGHTEFF, BACKL_MODE);
  EEPROM.get(GLEFF, GLITCH_ALLOWED);
  EEPROM.get(ALHOUR, alm_hrs);
  EEPROM.get(ALMIN, alm_mins);
  EEPROM.get(ALIFSET, alm_set);
  EEPROM.get(BLCOLOR, backlColor);
  EEPROM.get(AUTOSHOWMEAS, auto_show_measurements);

  // включаем ШИМ
  r_duty = DUTY;
  setPWM(GEN, r_duty);
  
  sendTime(hrs, mins, secs);                      // отправить время на индикаторы

  changeBright();                                 // изменить яркость согласно времени суток

  // стартовый период глюков
  glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));

  // скорость режима при запуске
  flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);

  // инициализация BME
  isBMEhere = bme.begin();
  if( !isBMEhere) {
    isBMEhere = bme.begin(BME280_ADDRESS_ALTERNATE);
  }
  if (isBMEhere ) bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X16, // temperature
                    Adafruit_BME280::SAMPLING_X16, // pressure
                    Adafruit_BME280::SAMPLING_X16, // humidity
                    Adafruit_BME280::FILTER_X4   );
}

/* Обеспечение работы подсветки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void backlBrightTick() 
{
  switch (curMode) 
  {
    case SHTIME:                                  // (0) отображение часов
      if (chBL) 
	  {
        chBL = false;
        digitalWrite(BACKLR, 0);
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLB, 0);
        if (BACKL_MODE == 1) 
		{
          setPWM(backlColors[backlColor], backlMaxBright);
        } 
		else if (BACKL_MODE == 2) 
		{
          digitalWrite(backlColors[backlColor], 0);
        }
      }
      if (BACKL_MODE == 0 && backlBrightTimer.isReady()) 
	  {
        if (backlMaxBright > 0) 
		{
          if (backlBrightDirection) 
		  {
            if (!backlBrightFlag) 
			{
              backlBrightFlag = true;
              backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
            }
            backlBrightCounter += BACKL_STEP;
            if (backlBrightCounter >= backlMaxBright) 
			{
              backlBrightDirection = false;
              backlBrightCounter = backlMaxBright;
            }
          } 
		  else 
		  {
            backlBrightCounter -= BACKL_STEP;
            if (backlBrightCounter <= BACKL_MIN_BRIGHT) 
			{
              backlBrightDirection = true;
              backlBrightCounter = BACKL_MIN_BRIGHT;
              backlBrightTimer.setInterval(BACKL_PAUSE);
              backlBrightFlag = false;
            }
          }
          setPWM(backlColors[backlColor], getPWM_CRT(backlBrightCounter));
        } 
		else 
		{
          digitalWrite(backlColors[backlColor], 0);
        }
      }
      break;
      
    case SETTIME:                                 // (1) установка часов
    case SHALARM:                                 // (2) отображение времени будильника (5 сек)
    case SETALARM:                                // (3) установка времени будильника
      if (chBL) 
	  {
        chBL = false;
        digitalWrite(backlColors[backlColor], 0);
      }
      break;
    
    case SHTEMP:                                  // (4) отображение температуры (5 сек)
      if (chBL) 
	  {
        chBL = false;
        setPWM(BACKLR, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLB, 0);
      }
      break;
    case SHHUM:                                   // (5) отображение влажности (5 сек)
      if (chBL) 
	  {
        chBL = false;
        setPWM(BACKLB, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLR, 0);
      }
      break;
    case SHATM:                                   // (6) отображение атмосферного давления (5 сек)
      if (chBL) 
		  {
        chBL = false;
        setPWM(BACKLG, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLR, 0);
        digitalWrite(BACKLB, 0);
      }
      break;
  }
}



/* Проигрывание мелодии для будильника (часть, работающая в основном коде)
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void beeper() {
  // бипер
  if (alm_flag) {
    if (!note_ip) {                   // первое включение звука
      if (NotePrescalerHigh[0]) {
                                      // переводим порт в высокое состояние только если это не пауза
        bitWrite(PORTD, PIEZO, 1);
        note_up_low = true;           // включаем высокий уровень
      } else {                        // это - пауза
        bitWrite(PORTD, PIEZO, 0);    // переводим порт в низкое состояние
        note_up_low = false;          // включаем низкий уровень
      }
      note_num = 0;                   // перемещаемся на начало мелодии
      note_count = 0;                 // перемещаемся в начало фазы сигнала
                                      // заводим таймер на длину ноты
      noteTimer.setInterval(NoteLength[note_num]);
      noteTimer.reset();
      note_ip = true;                 // признак включения сигнала
    } else if (noteTimer.isReady()) { // нота завершена
      if (++note_num == notecounter)
        note_num = 0;                 // мелодия закончилась, начинаем сначала
      note_count = 0;                 // перемещаемся в начало фазы сигнала
      noteTimer.setInterval(NoteLength[note_num]);
      if (NotePrescalerHigh[note_num]) {
                                      // переводим порт в высокое состояние только если это не пауза
        bitWrite(PORTD, PIEZO, 1);
        note_up_low = true;           // включаем высокий уровень
      } else {                        // это - пауза
        bitWrite(PORTD, PIEZO, 0);    // переводим порт в низкое состояние
        note_up_low = false;          // включаем низкий уровень
      }
    }
  } else {                            // нужно выключить сигнал
    if (note_ip) {                    // делаем это однократно за полсекунды
      note_ip = false;                
      if (note_up_low) {              // если есть напряжение на выходе
        note_up_low = false;          // сбрасываем индикатор напряжения
        bitWrite(PORTD, PIEZO, 0);    // переводим порт в низкое состояние
      }
    }
  }
}

/* Поведение отображаемых значений в режимах установки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void settingsTick() 
{
  if (curMode == SETTIME || curMode == SETALARM) 
  {
    if (curMode == SETALARM && !alm_set) 
    {          // мигать отображением времени будильника, если будильник не установлен
      if (!(anodeStates == 0 || anodeStates == 0xF)) anodeStates = 0;
      if (blinkTimer.isReady()) anodeStates ^= 0xF;
    } 
    else 
    {
      if (blinkTimer.isReady()) 
      {
        lampState = !lampState;
        if (lampState) anodeStates = 0xF;
        else if (!currentDigit) anodeStates = 0x0C;
        else anodeStates = 0x3; 
      }
    }
  }
}

/* Возврат к отображению времени
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void retToTime() 
{
  curMode = SHTIME;

  anodeStates = 0x3F;
  sendTime(hrs, mins, secs);

  dotSetMode( alm_set ? DOT_IN_ALARM : DOT_IN_TIME );
  chBL = true;
}

/* Обработка нажатий кнопок
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */

SH_MODES prev_curMode = SHATM;

inline void buttonsTick() 
{

  btnA.tick();                                    // определение, нажата ли кнопка Alarm
  // сначала - особый вариант реагирования на кнопки в режиме сработавшего будильника
  if (alm_flag) 
  {
    if (btnA.isClick() || btnA.isHolded()) alm_flag = false;
    return;
  }

  int analog = analogRead(A7);                    // чтение нажатой кнопки
  btnSet.tick(analog <= 1023 && analog > 950);    // определение, нажата ли кнопка Set
  btnL.tick(analog <= 860 && analog > 450);       // определение, нажата ли кнопка Up
  btnR.tick(analog <= 380 && analog > 100);       // определение, нажата ли кнопка Down

  switch (curMode) 
  {
    /*------------------------------------------------------------------------------------------------------------------------------*/
    case SHTIME:                                  // (0) отображение часов
      if (btnR.isClick())                         // переключение эффектов цифр
      {                       
        if (++FLIP_EFFECT >= FLIP_EFFECT_NUM) FLIP_EFFECT = FM_NULL;
        EEPROM.put(FLIPEFF, FLIP_EFFECT);
                                                  // для показа номера эффекта
        eshowTimer.reset();
        showFlag = true;
        memset((void*)indiDimm, indiMaxBright, NUMTUB);
        memset((void*)indiDigits, FLIP_EFFECT, NUMTUB);

        anodeStates = 0x3F;
        newSecFlag = true;
 
        newTimeFlag = true;
      }
      
      if (btnR.isHolded()) // автопоказывать измерения температуры, давления и влажности
      {
        auto_show_measurements = !auto_show_measurements;
        EEPROM.put(AUTOSHOWMEAS, auto_show_measurements);
      }

      if (btnL.isClick())                           // переключение эффектов подсветки
      {                       
        if (++BACKL_MODE >= 3) 
        {
          BACKL_MODE = 0;
          digitalWrite(backlColors[backlColor], 0);
          if(++backlColor >= 3) backlColor = 0;
          EEPROM.put(BLCOLOR, backlColor);
        }
        EEPROM.put(LIGHTEFF, BACKL_MODE);
        chBL = true;
      }

      if (btnL.isHolded()) // переключение глюков
      {                      
        GLITCH_ALLOWED = !GLITCH_ALLOWED;
        EEPROM.put(GLEFF, GLITCH_ALLOWED);
      }
      
      if (btnA.isClick() 
          || 
          btnA.isHolded() 
          || 
          autoShowMeasurementsTimer.isReadyDisable() 
         ) // переход в режим отображения температуры
      {    
        curMode = SHTEMP;
        if (isBMEhere) 
        { 
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          indiDigits[0] = (byte)((int)temp_event.temperature / 10);
          indiDigits[1] = (byte)((int)temp_event.temperature % 10);
          indiDigits[2] = (byte)((int)(temp_event.temperature * 10) % 10); 
        } 
        else 
        {
          indiDigits[0] = (byte) 0;
          indiDigits[1] = (byte) 0;
          indiDigits[2] = (byte) 0; 
        }
        measurementsTimer.reset();
        anodeStates = 0x07;
        autoTimer.setInterval(TEMP_SH_TIME);
        autoTimer.reset();
        dotSetMode( DM_NULL );
        chBL = true;
      }
      
      if (btnSet.isDouble()) // переход в режим установки времени
      {                    
        anodeStates = 0x0F;
        currentDigit = false;
        curMode = SETTIME;
        changeHrs = hrs;
        changeMins = mins;
        sendTime(changeHrs, changeMins, 0);

        chBL = true;
      }

      if (btnSet.isHolded()) // переход в режим установки будильника и времени его
      {                   
        anodeStates = 0x0F;
        currentDigit = false;
        curMode = SETALARM;
        changeHrs = alm_hrs;
        changeMins = alm_mins;
        
        sendTime(changeHrs, changeMins, 0);

        dotSetMode( DM_NULL );
        chBL = true;
      }

      break;
      
    /*------------------------------------------------------------------------------------------------------------------------------*/
    case SETTIME:                                 // (1) установка часов
    case SETALARM:                                // (3) установка времени будильника

      if (!(curMode == SETALARM && !alm_set)) 
      {
                                                  // переход между разрядами      
        if (btnSet.isClick()) currentDigit = !currentDigit;
        if (btnSet.isHolded()) 
        {                  // обнуление текущего рвазряда
          if (!currentDigit) 
          {
            changeHrs = 0;
          }
          else 
          {
            changeMins = 0;
          }
          sendTime(changeHrs, changeMins, 0);
        }

        if (btnL.isClick())  // уменьшить значение
        {                    
          if (!currentDigit) 
          {
            changeHrs--;
            if (changeHrs < 0) changeHrs = 23;
          } 
          else 
          {
            changeMins--;
            if (changeMins < 0) 
            {
              changeMins = 59;
              changeHrs--;
              if (changeHrs < 0) changeHrs = 23;
            }
          }
          sendTime(changeHrs, changeMins, 0);
        }

        if (btnL.isHolded()) // уменьшить значение на 5
        {                     
          if (!currentDigit) 
          {
            changeHrs -= 5;
            if (changeHrs < 0) changeHrs += 24;
          } 
          else 
          {
            changeMins -= 5;
            if (changeMins < 0) 
            {
              changeMins += 60;
              changeHrs--;
              if (changeHrs < 0) changeHrs = 23;
            }
          }
          sendTime(changeHrs, changeMins, 0);
        }
      
        if (btnR.isClick()) // увеличить значение
        {                     
          if (!currentDigit) 
          {
            changeHrs++;
            if (changeHrs > 23) changeHrs = 0;
          } 
          else 
          {
            changeMins++;
            if (changeMins > 59) 
            {
              changeMins = 0;
              changeHrs++;
              if (changeHrs > 23) changeHrs = 0;
            }
          }
          sendTime(changeHrs, changeMins, 0);
        }
      
        if (btnR.isClick()) // увеличить значение на 5
        {                     
          if (!currentDigit) 
          {
            changeHrs += 5;
            if (changeHrs > 23) changeHrs -= 24;
          } 
          else 
          {
            changeMins += 5;
            if (changeMins > 59) 
            {
              changeMins -= 60;
              changeHrs++;
              if (changeHrs > 23) changeHrs = 0;
            }
          }
          sendTime(changeHrs, changeMins, 0);
        }
      }
                                                  
      if (btnA.isHolded()) // переход в режим отображения времени без сохранения или смена установки будильника
      {                      
        if (curMode == SETTIME) 
          retToTime();
        else alm_set = !alm_set;
      }

      if (btnA.isClick()) 
      {
        if (curMode == SETTIME) 
        {
          hrs = changeHrs;
          mins = changeMins;
          secs = 0;
          DateTime now = rtc.now();
          rtc.adjust(DateTime(now.year(), now.month(), now.day(), hrs, mins, 0));
          SQW_counter = 0;
          changeBright(); 
        } 
        else 
        {
          alm_hrs = changeHrs;
          alm_mins = changeMins;
          EEPROM.put(ALHOUR, alm_hrs);
          EEPROM.put(ALMIN, alm_mins);
          EEPROM.put(ALIFSET, alm_set);
        }
        retToTime();
      }

      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHALARM:                                 // (2) отображение времени будильника (5 сек)
      if (autoTimer.isReady() || btnA.isClick() || btnA.isHolded()) retToTime();
      
      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHTEMP:                                  // (4) отображение температуры (5 сек)
      if (measurementsTimer.isReady()) 
      {
        if (isBMEhere) 
        { 
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          indiDigits[0] = (byte)((int)temp_event.temperature / 10);
          indiDigits[1] = (byte)((int)temp_event.temperature % 10);
          indiDigits[2] = (byte)((int)(temp_event.temperature * 10) % 10); 
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) 
      {
        curMode = SHATM;
        if (isBMEhere) 
        {
          float pressure_in_mm = pressure_event.pressure / 1.333223684;
          indiDigits[1] = (byte)((int)pressure_in_mm / 100);
          indiDigits[2] = (byte)(((int)pressure_in_mm / 10) % 10);
          indiDigits[3] = (byte)((int)pressure_in_mm % 10);
        } 
        else 
        {
          indiDigits[1] = (byte)0;
          indiDigits[2] = (byte)0;
          indiDigits[3] = (byte)0;
        }
        anodeStates = 0x0E;
        dotSetMode( DM_NULL );
        autoTimer.setInterval(ATMOSPHERE_SH_TIME);
        autoTimer.reset();
        chBL = true;
      }
      
      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHHUM:                                   // (5) отображение влажности (5 сек)
      if (measurementsTimer.isReady()) 
      {
        if (isBMEhere) 
        {
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          if ((int)humidity_event.relative_humidity == 100) 
          {
            indiDigits[4] = indiDigits[5] = 9;
          }
          else 
          {
            indiDigits[4] = (byte)((int)humidity_event.relative_humidity / 10);
            indiDigits[5] = (byte)((int)humidity_event.relative_humidity % 10);
          }
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) 
      {
        if (alm_set) 
        {
          curMode = SHALARM;
          anodeStates = 0x0F;
          sendTime(alm_hrs, alm_mins, 0);
          autoTimer.setInterval(ALARM_SH_TIME);
          autoTimer.reset();
          dotSetMode( DM_NULL );
          chBL = true;
        } 
        else retToTime();
      }
      break;
      
    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHATM:                                   // (6) отображение атмосферного давления (5 сек)
      if (measurementsTimer.isReady()) 
      {
        if (isBMEhere) 
        {
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          float pressure_in_mm = pressure_event.pressure / 1.333223684;
          indiDigits[1] = (byte)((int)pressure_in_mm / 100);
          indiDigits[2] = (byte)(((int)pressure_in_mm / 10) % 10);
          indiDigits[3] = (byte)((int)pressure_in_mm % 10);
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) 
      {
        curMode = SHHUM;
        if (isBMEhere) 
        {
          if ((int)humidity_event.relative_humidity == 100) 
          {
            indiDigits[4] = indiDigits[5] = 9;
          }
          else 
          {
            indiDigits[4] = (byte)((int)humidity_event.relative_humidity / 10);
            indiDigits[5] = (byte)((int)humidity_event.relative_humidity % 10);
          }
        } 
        else 
        {
          indiDigits[4] = indiDigits[5] = 0;
        }
        anodeStates = 0x30;
        autoTimer.setInterval(HUMIDITY_SH_TIME);
        autoTimer.reset();
        chBL = true;
      }
      
      break;
  }
}


/* Стабилизация высокого напряжения
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void DCDCTick() {
  int voltage;
  
  voltage = analogRead(A6);                                               // читаем значение анодного напряжения
  if(startup_delay <=0) {                                                 // ждём первоначальной установки напряжения
    if(++idcounter == iduty) {                                            // пройден полный цикл усреднения
      duty_delta = voltage - nominallevel;                                // текущая разница с идеальным напряжением - первая
      idcounter = 0;                                                      // обновляем цикл усреднения
    } else duty_delta += voltage - nominallevel;                          // интегрируем ошибку
    if ( duty_delta > 0 && duty_delta > maxerrduty ) {                    // измеренное напряжение выше идеального и превышает допустимую ошибку
      duty_delta = 0;                                                     // обнуляем интегральную ошибку
      if (r_duty > minduty) {                                             // если есть ещё возможность уменьшить длину активного импульса
        setPWM(GEN, --r_duty);                                            // уменьшаем длину активного импульса и устанавливаем новые значения ШИМ
      } else setPWM(GEN, 0);                                              // иначе выключаем генерацию
    } else if ( duty_delta < 0 && duty_delta < -maxerrduty ) {            // измеренное значение ниже идеального и превышает допустимую ошибку
      duty_delta = 0;                                                     // обнуляем интегральную ошибку
      if (++r_duty > maxduty ) r_duty = maxduty;                          // если нет возможности увеличить длину активного импульса - оставляем максимальную
      else {
        setPWM(GEN, r_duty);                                              // если есть возможности увеличить длину активного импульса - устанавливаем новый ШИМ
      }
    }
  }
}

/* Установка режима работы секундной точки
 *  Входные параметры:
 *    DOT_MODES dMode: устанавливаемый режим работы секундной точки
 *  Выходные параметры: нет
 */
void dotSetMode(DOT_MODES dMode) {
  dotMode = dMode;
  dotBrightFlag = false;  
  switch(dMode) {
    case DM_NULL:
      setPWM(DOT, getPWM_CRT(0));
      break;
    case DM_ONCE:
    case DM_HALF:
    case DM_TWICE:
    case DM_THREE:
      dotNumBlink = 0;
      // расчёт шага яркости точки
      dotBrightStep = ceil((float)dotMaxBright * 2 / DOT_TIME * DOT_TIMER);
      if (dotBrightStep == 0) dotBrightStep = 1;
      if (dMode == DM_TWICE) dotBrightStep *= 2;
      else if (dMode == DM_THREE) dotBrightStep *= 3;
      break;
    case DM_FULL:
      setPWM(DOT, getPWM_CRT(dotMaxBright));
      break;
  }
}

/* Обеспечение работы секундной точки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void dotBrightTick() {
  if (dotMode == DM_NULL || dotMode == DM_FULL) return;
  if (dotBrightFlag && dotBrightTimer.isReady()) {
    if (dotBrightDirection) {
      dotBrightCounter += dotBrightStep;
      if (dotBrightCounter >= dotMaxBright) {
        dotBrightDirection = false;
        dotBrightCounter = dotMaxBright;
      }
    } else {
      dotBrightCounter -= dotBrightStep;
      if (dotBrightCounter <= ((dotMode == DM_HALF) ? dotMaxBright / 2 : 0)) {
        dotBrightDirection = true;
        if (dotMode == DM_TWICE || dotMode == DM_THREE) {
          dotNumBlink++;
          if (dotNumBlink == (dotMode == DM_TWICE ? 2 : 3) ) {
            dotNumBlink = 0;
            dotBrightFlag = false;
          }
        } else dotBrightFlag = false;
        dotBrightCounter = ((dotMode == DM_HALF) ? dotMaxBright / 2 : 0);
      }
    }
    setPWM(DOT, getPWM_CRT(dotBrightCounter));
  }
}

/* Обеспечение отображения эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void flipTick() 
{
  if (FLIP_EFFECT == FM_NULL) 
  {
    sendTime(hrs, mins, secs);
    newTimeFlag = false;
    newSecFlag = false;
  }
  else if (FLIP_EFFECT == FM_SMOOTH) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (newSecFlag) 
      {                              // если изменились секунды во время эффекта - меняем их отображение
        newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
        indiDigits[4] = (byte)secs / 10;
        indiDigits[5] = (byte)secs % 10;
      }
      if (!flipInit) 
      {
        flipInit = true;
        flipTimer.setInterval((unsigned long)FLIP_SPEED[FLIP_EFFECT]);
        flipTimer.reset();
        indiBrightDirection = false;                 // задаём направление
        // запоминаем, какие цифры поменялись и будем менять их яркость
        for (byte i = 0; i < NUMTUB; i++) 
        {
          if (indiDigits[i] != newTime[i]) flipIndics[i] = true;
          else flipIndics[i] = false;
        }
      }
      if (flipTimer.isReady()) 
      {
        if (!indiBrightDirection) 
        {
          indiBrightCounter--;                        // уменьшаем яркость
          if (indiBrightCounter <= 0) 
          {               // если яркость меньше нуля
            indiBrightDirection = true;               // меняем направление изменения
            indiBrightCounter = 0;                    // обнуляем яркость
            sendTime(hrs, mins, secs);                // меняем цифры
          }
        } 
        else 
        {
          indiBrightCounter++;                        // увеличиваем яркость
          if (indiBrightCounter >= indiMaxBright) {   // достигли предела
            indiBrightDirection = false;              // меняем направление
            indiBrightCounter = indiMaxBright;        // устанавливаем максимум
            // выходим из цикла изменения
            flipInit = false;
            newTimeFlag = false;
          }
        }
                                                      // применяем яркость
        for (byte i = 0; i < NUMTUB; i++) if (flipIndics[i]) indiDimm[i] = indiBrightCounter;
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }
  else if (FLIP_EFFECT == FM_LIST) 
  {
    if (!flipInit) 
    {
      flipInit = true;
      flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
                                                    // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (indiDigits[i] != newTime[i]) 
          flipIndics[i] = true;
        else 
          flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) 
    {
      byte flipCounter = 0;
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (flipIndics[i]) 
        {
          indiDigits[i]--;
          if (indiDigits[i] < 0) indiDigits[i] = 9;
          if (indiDigits[i] == newTime[i]) flipIndics[i] = false;
        } 
        else 
        {
          flipCounter++;                            // счётчик цифр, которые не надо менять
        }
      }
      if (flipCounter == NUMTUB) 
      {                  // если ни одну из 6 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
        newSecFlag = false;
      }
    }
  }
  else if (FLIP_EFFECT == FM_CATHODE) 
  {
    if (!flipInit) 
    {
      flipInit = true;
      flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
                                                    // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (indiDigits[i] != newTime[i]) 
        {
          flipIndics[i] = true;
          for (byte c = 0; c < 10; c++) 
          {
            if (cathodeMask[c] == indiDigits[i]) startCathode[i] = c;
            if (cathodeMask[c] == newTime[i]) endCathode[i] = c;
          }
        }
        else flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) 
    {
      byte flipCounter = 0;
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (flipIndics[i]) 
        {
          if (startCathode[i] > endCathode[i]) 
          {
            startCathode[i]--;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } 
          else if (startCathode[i] < endCathode[i]) 
          {
            startCathode[i]++;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } 
          else 
          {
            flipIndics[i] = false;
          }
        } 
        else 
        {
          flipCounter++;
        }
      }
      if (flipCounter == NUMTUB) 
      {                  // если ни одну из 6 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
        newSecFlag = false;
      }
    }
  }
// --- train --- //
  else if (FLIP_EFFECT == FM_TRAIN) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (!flipInit) 
      {
        flipInit = true;
        currentLamp = 0;
        trainLeaving = true;
        flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
        //flipTimer.reset();
      }
      if (flipTimer.isReady()) 
      {
        if (trainLeaving) 
        {
          for (byte i = NUMTUB-1; i > currentLamp; i--) 
          {
            indiDigits[i] = indiDigits[i-1];
          }
          anodeStates &= ~(1<<currentLamp);
          currentLamp++;
          if (currentLamp >= NUMTUB) 
          {
            trainLeaving = false;                     //coming
            currentLamp = 0;
          }
        }
        else 
        {                                        //trainLeaving == false
          for (byte i = currentLamp; i > 0; i--) 
          {
            indiDigits[i] = indiDigits[i-1];
          }
          indiDigits[0] = newTime[NUMTUB-1-currentLamp];
          anodeStates |= 1<<currentLamp;
          currentLamp++;
          if (currentLamp >= NUMTUB) 
          {
            flipInit = false;
            newTimeFlag = false;
          }
        }
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }

// --- elastic band --- //
  else if (FLIP_EFFECT == FM_ELASTIC) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (!flipInit) 
      {
        flipInit = true;
        flipEffectStages = 0;
        flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
        // flipTimer.reset();
      }
      if (flipTimer.isReady()) 
      {
        switch (flipEffectStages++) 
        {
          case 1:
          case 3:
          case 6:
          case 10:
          case 15:
          case 21:
            anodeStates &= ~(1<<5);
            break;
          case 2:
          case 5:
          case 9:
          case 14:
          case 20:
            anodeStates &= ~(1<<4); indiDigits[5] = indiDigits[4]; anodeStates |= 1<<5;
            break;
          case 4:
          case 8:
          case 13:
          case 19:
            anodeStates &= ~(1<<3); indiDigits[4] = indiDigits[3]; anodeStates |= 1<<4;
            break;
          case 7:
          case 12:
          case 18:
            anodeStates &= ~(1<<2); indiDigits[3] = indiDigits[2]; anodeStates |= 1<<3;
            break;
          case 11:
          case 17:
            anodeStates &= ~(1<<1); indiDigits[2] = indiDigits[1]; anodeStates |= 1<<2;
            break;
          case 16:
            anodeStates &= ~(1<<0); indiDigits[1] = indiDigits[0]; anodeStates |= 1<<1;
            break;
          case 22:
            indiDigits[0] = newTime[5]; anodeStates |= 1<<0;
            break;
          case 23:
          case 29:
          case 34:
          case 38:
          case 41:
            anodeStates &= ~(1<<0); indiDigits[1] = indiDigits[0]; anodeStates |= 1<<1;
            break;
          case 24:
          case 30:
          case 35:
          case 39:
            anodeStates &= ~(1<<1); indiDigits[2] = indiDigits[1]; anodeStates |= 1<<2;
            break;
          case 25:
          case 31:
          case 36:
            anodeStates &= ~(1<<2); indiDigits[3] = indiDigits[2]; anodeStates |= 1<<3;
            break;
          case 26:
          case 32:
            anodeStates &= ~(1<<3); indiDigits[4] = indiDigits[3]; anodeStates |= 1<<4;
            break;
          case 27:
            anodeStates &= ~(1<<4); indiDigits[5] = indiDigits[4]; anodeStates |= 1<<5;
            break;
          case 28:
            indiDigits[0] = newTime[4]; anodeStates |= 1<<0;
            break;
          case 33:
            indiDigits[0] = newTime[3]; anodeStates |= 1<<0;
            break;
          case 37:
            indiDigits[0] = newTime[2]; anodeStates |= 1<<0;
            break;
          case 40:
            indiDigits[0] = newTime[1]; anodeStates |= 1<<0;
            break;        
          case 42:
            indiDigits[0] = newTime[0]; anodeStates |= 1<<0;
            break;
          case 43:
            flipInit = false;
            newTimeFlag = false;
        }
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }
}


/* check 28.10.20 
 *  
 */

/* Обеспечение "антиотравления"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void burnIndicators() {
  for (byte k = 0; k < BURN_LOOPS; k++) {
    for (byte d = 0; d < 10; d++) {
      for (byte i = 0; i < NUMTUB; i++) {
        indiDigits[i]--;
        if (indiDigits[i] < 0) indiDigits[i] = 9;
      }
      delay((unsigned long)BURN_TIME);
    }
  }
}

/* check 28.10.20 */

/* Обеспечение работы "глюков"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void glitchTick() {
  if (!glitchFlag && secs > 7 && secs < 55) {
    if (glitchTimer.isReady()) {
      glitchFlag = true;
      indiState = 0;
      glitchCounter = 0;
      glitchMax = random(2, 6);
      glitchIndic = random(0, NUMTUB);
      glitchTimer.setInterval(random(1, 6) * 20);
    }
  } else if (glitchFlag && glitchTimer.isReady()) {
    indiDimm[glitchIndic] = indiState * indiMaxBright;
    indiState = !indiState;
    glitchTimer.setInterval(random(1, 6) * 20);
    glitchCounter++;
    if (glitchCounter > glitchMax) {
      glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));
      glitchFlag = false;
      indiDimm[glitchIndic] = indiMaxBright;
    }
  }
}
/* check 28.10.20
 *  
*/

/* Обработчик прерывания SQW
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void RTC_handler() {
  // таймер
  if (++SQW_counter == 4096) 
  {
    halfsecond = true;  // Прошло полсекунды
  }
  else if (SQW_counter == 8192) 
  {     // Прошла секунда
    SQW_counter = 0;                  // Начинаем новую секунду
    halfsecond = true;                // Полсекундный индикатор
  }
  
  // бипер (замена tone() по причине перенастройки базовых таймеров для его работы)
  if (note_ip) 
  {                      // будильник включен
    if (note_up_low) 
    {                // положительная полуволна
      if (++note_count >= NotePrescalerHigh[note_num]) 
      {
                                      // достигли длительности положительной полуволны
        note_up_low = false;          // переходим на отрицательную полуволну
        note_count = 0;               // сбрасываем счётчик
        bitWrite(PORTD, PIEZO, 0);    // переводим порт в низкое состояние
      }
    } 
    else 
    {                          // отрицательная полуволна
      if (++note_count >= NotePrescalerLow[note_num]) 
      {
                                      // достигли длительности отрицательной полуволны
        note_count = 0;               // сбрасываем счётчик
        if (NotePrescalerHigh[note_num]) 
        {
                                      // при ненулевой длительности положительной полуволны
          note_up_low = true;         // переходим на положительную полуволну
          bitWrite(PORTD, PIEZO, 1);  // переводим порт в высокое состояние
        }
      }
    }
  }
  
  indiCounter[curIndi]++;             // счётчик индикатора
  if (indiCounter[curIndi] >= indiDimm[curIndi])  // если достигли порога диммирования
    setPin(opts[curIndi], 0);         // выключить текущий индикатор

  if (indiCounter[curIndi] > 25) 
  {    // достигли порога в 25 единиц
    indiCounter[curIndi] = 0;         // сброс счетчика лампы
    if (++curIndi >= NUMTUB) curIndi = 0;  // смена лампы закольцованная

    // отправить цифру из массива indiDigits согласно типу лампы
    if (indiDimm[curIndi] > 0) 
    {
      byte thisDig = digitMask[indiDigits[curIndi]];
      setPin(DECODER3, bitRead(thisDig, 0));
      setPin(DECODER1, bitRead(thisDig, 1));
      setPin(DECODER0, bitRead(thisDig, 2));
      setPin(DECODER2, bitRead(thisDig, 3));
      setPin(opts[curIndi], anodeStates & (1<<curIndi) );    // включить анод на текущую лампу
    }
  }
}

/* check 28.10.20 */
/* 
 */

/* Обеспечение хода времени и запуск зависящих от времени процессов
 *  ("антиотравления" катодов, синхронизация с RTC (в 3 часа ночи),
 *  работа будильника)
 *  
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void calculateTime() 
{
  halfsecond = false;
  dotFlag = !dotFlag;
  if (dotFlag) 
  {
    dotBrightFlag = true;
    dotBrightDirection = true;
    dotBrightCounter = 0;
    newSecFlag = true;
    if (startup_delay) startup_delay--;
    // синхронизация с RTC каждую секунду
    {                                   
      boolean time_sync = false;
      DateTime now = rtc.now();
      do 
      {
        if (!time_sync) 
        {
          time_sync = true;
          secs = now.second();
          mins = now.minute();
          hrs = now.hour();
        }
        now = rtc.now();
      } while (secs != now.second());
      secs = now.second();
      mins = now.minute();
      hrs = now.hour();
    }

    if (auto_show_measurements)
    {
      if (secs == 20 || secs == 40 ) // каждые 20 и 40 секунд запускается таймер на автопоказ измерений
      {      
        autoShowMeasurementsTimer.setInterval(10);
        autoShowMeasurementsTimer.reset();
      }
    }

    if (secs >= 59) 
    {
      newTimeFlag = true;                                // флаг что нужно поменять время (минуты и часы)
      alm_request = true;                                // нужно проверить будильник
      if (mins % BURN_PERIOD == 0) burnIndicators();     // чистим чистим!
      changeBright();
    }

    if (newTimeFlag || newSecFlag) setNewTime();        // обновляем массив времени

    if (alm_request) 
    {                                  // при смене минуты
      alm_request = false;                              // сбрасываем признак
      if (alm_fired) alm_fired = false;                 // в следующую минуту - сбрасываем признак звучания будильника
      if (alm_set && !alm_fired) 
      {                      // если установлен будильник и он не звучал в текущую минуту
                                                        // во всех режимах, кроме установки будильника, сравниваем время с установленным в будильнике
        if (hrs == alm_hrs && mins == alm_mins && curMode != SETALARM) 
        {
          alm_fired = true;                             // будильник звучит в текущую минуту
          alm_flag = true;                              // будильник сработал
          almTimer.reset();                             // запуск таймера предельного звучания будильника
        }
      }
    }
    if (alm_flag) 
    {                                     // если звучит будильник
      if (almTimer.isReady()) 
      {                         // ждём предельного времени звучания будильника
        alm_flag = false;                               // сбрасываем сработку будильника
      }
    }
  }
}


void loop() {


  if (halfsecond) calculateTime();                // каждые 500 мс пересчёт и отправка времени
  beeper();
  if (showFlag) 
  {                                 // отображение номера эффекта для цифр при переходе
    if(eshowTimer.isReady()) showFlag = false;
  } 
  else 
  {
    if ((newSecFlag || newTimeFlag) && curMode == 0) 
    {
      flipTick();     // перелистывание цифр
    }
  }
  dotBrightTick();                                // плавное мигание точки
  backlBrightTick();                              // плавное мигание подсветки ламп
  if (GLITCH_ALLOWED && curMode == SHTIME) glitchTick();  // глюки
  buttonsTick();                                  // кнопки
  settingsTick();                                 // настройки
  DCDCTick();                                     // анодное напряжение
}

int main(void)
{
	init();

	initVariant();

#if defined(USBCON)
	USBDevice.attach();
#endif
	
	setup();
    
	for (;;) {
		loop();
		if (serialEventRun) serialEventRun();
	}
        
	return 0;
}

