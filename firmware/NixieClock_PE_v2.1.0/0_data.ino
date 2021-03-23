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
#define ALARM_SH_TIME       5000                  // время отображения установленного будильника
#define TEMP_SH_TIME        6000                  // время показа температуры до перехода к влажности
#define HUMIDITY_SH_TIME    6000                  // время показа влажности до перехода к давлению
#define ATMOSPHERE_SH_TIME  6000                  // время показа давления до возврата к отображению времени
#define MEASURE_PERIOD      2000                  // период обновления показаний
timerMinim autoTimer(ALARM_SH_TIME);              // таймер автоматического выхода из режимов
timerMinim measurementsTimer(MEASURE_PERIOD);     // таймер обновления показаний

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
const int nominallevel = 550;                     // номинальное значение напряжения
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
                     BLCOLOR };                   // (7) текущий цвет подсветки

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
