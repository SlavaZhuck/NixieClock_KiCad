
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"


// распиновка ламп
static const byte digitMask[] = {8, 9, 0, 1, 5, 2, 4, 6, 7, 3};  // маска дешифратора платы COVID-19 (подходит для ИН-14 и ИН-12)
static const byte opts[] = {KEY0, KEY1, KEY2, KEY3, KEY4, KEY5}; // порядок индикаторов слева направо

/* мелодия */
// мелодия (длительность импульса, длительность паузы - в циклах таймера, длительность ноты - в мс)
static uint8_t NotePrescalerLow[] = {25, 255, 25, 21, 25, 28, 32, 33, 25, 255, 25, 21, 25, 28, 32, 28, 32, 33};

static volatile int8_t indiCounter[NUMTUB]; // счётчик каждого индикатора (0-24)
static volatile int8_t curIndi;             // текущий индикатор (0-5)

// длительность мелодии в нотах
static const uint16_t NoteLength[] = {L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 8, L16 * 8, L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 8};

static timerMinim noteTimer(L16); // таймер длительности ноты

// мелодия (длительность импульса, длительность паузы - в циклах таймера, длительность ноты - в мс)
static const uint8_t NotePrescalerHigh[] = {25, 0, 25, 21, 25, 28, 31, 33, 25, 0, 25, 21, 25, 28, 31, 28, 31, 33};
static const uint8_t notecounter = sizeof(NotePrescalerHigh);

static volatile boolean note_up_low = false; // true - высокий уровень, false - низкий уровень
static volatile boolean note_ip = false;     // true - сигнал звучит, false - сигнал не звучит
static volatile unsigned int note_num = 0;   // номер ноты в мелодии
static volatile unsigned int note_count = 0; // фаза сигнала

/* check 28.10.20
 *
 */

/* Обработчик прерывания SQW
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void RTC_handler()
{
  // таймер
  if (++SQW_counter == 4096)
  {
    halfsecond = true; // Прошло полсекунды
  }
  else if (SQW_counter == 8192)
  {                    // Прошла секунда
    SQW_counter = 0;   // Начинаем новую секунду
    halfsecond = true; // Полсекундный индикатор
  }

  // бипер (замена tone() по причине перенастройки базовых таймеров для его работы)
  if (note_ip)
  { // будильник включен
    if (note_up_low)
    { // положительная полуволна
      if (++note_count >= NotePrescalerHigh[note_num])
      {
        // достигли длительности положительной полуволны
        note_up_low = false;       // переходим на отрицательную полуволну
        note_count = 0;            // сбрасываем счётчик
        bitWrite(PORTD, PIEZO, 0); // переводим порт в низкое состояние
      }
    }
    else
    { // отрицательная полуволна
      if (++note_count >= NotePrescalerLow[note_num])
      {
        // достигли длительности отрицательной полуволны
        note_count = 0; // сбрасываем счётчик
        if (NotePrescalerHigh[note_num])
        {
          // при ненулевой длительности положительной полуволны
          note_up_low = true;        // переходим на положительную полуволну
          bitWrite(PORTD, PIEZO, 1); // переводим порт в высокое состояние
        }
      }
    }
  }

  indiCounter[curIndi]++;                        // счётчик индикатора
  if (indiCounter[curIndi] >= indiDimm[curIndi]) // если достигли порога диммирования
    setPin(opts[curIndi], 0);                    // выключить текущий индикатор

  if (indiCounter[curIndi] > 25)
  {                           // достигли порога в 25 единиц
    indiCounter[curIndi] = 0; // сброс счетчика лампы
    if (++curIndi >= NUMTUB)
      curIndi = 0; // смена лампы закольцованная

    // отправить цифру из массива indiDigits согласно типу лампы
    if (indiDimm[curIndi] > 0)
    {
      byte thisDig = digitMask[indiDigits[curIndi]];
      setPin(DECODER3, bitRead(thisDig, 0));
      setPin(DECODER1, bitRead(thisDig, 1));
      setPin(DECODER0, bitRead(thisDig, 2));
      setPin(DECODER2, bitRead(thisDig, 3));
      setPin(opts[curIndi], anodeStates & (1 << curIndi)); // включить анод на текущую лампу
    }
  }
}


/* Проигрывание мелодии для будильника (часть, работающая в основном коде)
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void beeper(void)
{
  // бипер
  if (alm_flag)
  {
    if (!note_ip)
    { // первое включение звука
      if (NotePrescalerHigh[0])
      {
        // переводим порт в высокое состояние только если это не пауза
        bitWrite(PORTD, PIEZO, 1);
        note_up_low = true; // включаем высокий уровень
      }
      else
      {                            // это - пауза
        bitWrite(PORTD, PIEZO, 0); // переводим порт в низкое состояние
        note_up_low = false;       // включаем низкий уровень
      }
      note_num = 0;   // перемещаемся на начало мелодии
      note_count = 0; // перемещаемся в начало фазы сигнала
                      // заводим таймер на длину ноты
      noteTimer.setInterval(NoteLength[note_num]);
      noteTimer.reset();
      note_ip = true; // признак включения сигнала
    }
    else if (noteTimer.isReady())
    { // нота завершена
      if (++note_num == notecounter)
        note_num = 0; // мелодия закончилась, начинаем сначала
      note_count = 0; // перемещаемся в начало фазы сигнала
      noteTimer.setInterval(NoteLength[note_num]);
      if (NotePrescalerHigh[note_num])
      {
        // переводим порт в высокое состояние только если это не пауза
        bitWrite(PORTD, PIEZO, 1);
        note_up_low = true; // включаем высокий уровень
      }
      else
      {                            // это - пауза
        bitWrite(PORTD, PIEZO, 0); // переводим порт в низкое состояние
        note_up_low = false;       // включаем низкий уровень
      }
    }
  }
  else
  { // нужно выключить сигнал
    if (note_ip)
    { // делаем это однократно за полсекунды
      note_ip = false;
      if (note_up_low)
      {                            // если есть напряжение на выходе
        note_up_low = false;       // сбрасываем индикатор напряжения
        bitWrite(PORTD, PIEZO, 0); // переводим порт в низкое состояние
      }
    }
  }
}