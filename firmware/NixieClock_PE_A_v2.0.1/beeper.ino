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
