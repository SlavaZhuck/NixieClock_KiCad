
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"

// длительность мелодии в нотах
static const uint16_t NoteLength[] = {L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 8, L16 * 8, L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 3, L16 * 3, L16 * 2, L16 * 8};

static timerMinim noteTimer(L16); // таймер длительности ноты

// мелодия (длительность импульса, длительность паузы - в циклах таймера, длительность ноты - в мс)
const uint8_t NotePrescalerHigh[] = {25, 0, 25, 21, 25, 28, 31, 33, 25, 0, 25, 21, 25, 28, 31, 28, 31, 33};
const uint8_t notecounter = sizeof(NotePrescalerHigh);

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