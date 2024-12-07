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

#include "effects.h"
#include "buttons.h"
#include "calculate_time.h"
#include "setup.h"
#include "set_time.h"
#include "DCDC.h"
#include "brightness.h"

// периферия
RTC_DS3231 rtc;                                   // создаём класс управления DS3231 (используем интерфейс I2C)
Adafruit_BME280 bme;                              // создаём класс управления BME (используем интерфейс I2C)
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

byte FLIP_SPEED[] = {0, 60, 50, 40, 90, 90}; 
byte FLIP_EFFECT = FM_SMOOTH;                               // Выбранный активен при первом запуске и меняется кнопками.

// таймеры



timerMinim flipTimer(FLIP_SPEED[FLIP_EFFECT]);    // таймер эффектов
timerMinim glitchTimer(1000);                     // таймер глюков
timerMinim blinkTimer(500);                       // таймер моргания
#define L16   127                                 // длительность 1/16 в мс при темпе 118 четвёртых в минуту
timerMinim noteTimer(L16);                        // таймер длительности ноты
timerMinim eshowTimer(300);                       // таймер демонстрации номера эффекта

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




int8_t hrs, mins, secs;                           // часы, минуты, секунды
int8_t alm_hrs, alm_mins;                         // сохранение времени будильника
boolean alm_set;                                  // будильник установлен? (берётся из памяти)
boolean alm_flag = ALARM_WAIT;                    // будильник сработал?



/* всё про подсветку */
byte backlColors[3] = { BACKLR, BACKLG, BACKLB };
byte backlColor;



/* всё про bme280 */
boolean isBMEhere;

boolean chBL = false;                             // для обнаружения необходимости смены подсветки

/* переменнные из исходного скетча */
boolean changeFlag;
boolean blinkFlag;
byte indiMaxBright = INDI_BRIGHT, backlMaxBright = BACKL_BRIGHT;
boolean backlBrightFlag, backlBrightDirection;
int backlBrightCounter, indiBrightCounter;
boolean newTimeFlag;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
boolean newSecFlag;                               // добавлен для исключения секунд из части эффектов
#endif

byte newTime[NUMTUB];


byte glitchCounter, glitchMax, glitchIndic;
boolean glitchFlag, indiState;

/* дополнительные переменные */
boolean showFlag = false;                         // признак демонстрации номера эффекта


SH_MODES curMode = SHTIME;

                     
boolean currentDigit = false;

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


// *********************** ДЛЯ РАЗРАБОТЧИКОВ ***********************
byte BACKL_MODE = 0;                                        // Выбранный режим активен при запуске и меняется кнопками
                                                            // скорость эффектов, мс (количество не меняй)
                                                            // количество эффектов
byte FLIP_EFFECT_NUM = sizeof(FLIP_SPEED);    
boolean GLITCH_ALLOWED = 1;                                 // 1 - включить, 0 - выключить глюки. Управляется кнопкой
boolean auto_show_measurements = 1;                         // автоматически показывать измерения Temp, Bar, Hum

// распиновка ламп
const byte digitMask[] = {8, 9, 0, 1, 5, 2, 4, 6, 7, 3};    // маска дешифратора платы COVID-19 (подходит для ИН-14 и ИН-12)
const byte opts[] = {KEY0, KEY1, KEY2, KEY3, KEY4, KEY5};   // порядок индикаторов слева направо


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

/* check 28.10.20 
 *  
 */

/* Обеспечение "антиотравления"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
 void burnIndicators() {
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

