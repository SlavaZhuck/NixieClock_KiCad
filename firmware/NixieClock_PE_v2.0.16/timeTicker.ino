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
void calculateTime() {
  halfsecond = false;
  dotFlag = !dotFlag;
  if (dotFlag) {
    dotBrightFlag = true;
    dotBrightDirection = true;
    dotBrightCounter = 0;
    secs++;
    newSecFlag = true;
    if (startup_delay) startup_delay--;
    if (secs > 59) {
      secs = 0;
      mins++;
      newTimeFlag = true;                                // флаг что нужно поменять время (минуты и часы)
      alm_request = true;                                // нужно проверить будильник
      if (mins % BURN_PERIOD == 0) burnIndicators();     // чистим чистим!
    }
    if (mins > 59) {
      mins = 0;
      hrs++;
      if (hrs > 23) hrs = 0;
      changeBright();
      if (hrs == 3) {                                   // синхронизация с RTC в 3 часа ночи
        boolean time_sync = false;
        DateTime now = rtc.now();
        do {
          if (!time_sync) {
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
    }
    if (newTimeFlag || newSecFlag) setNewTime();        // обновляем массив времени
    
    if (alm_request) {                                  // при смене минуты
      alm_request = false;                              // сбрасываем признак
      if (alm_fired) alm_fired = false;                 // в следующую минуту - сбрасываем признак звучания будильника
      if (alm_set && !alm_fired) {                      // если установлен будильник и он не звучал в текущую минуту
                                                        // во всех режимах, кроме установки будильника, сравниваем время с установленным в будильнике
        if (hrs == alm_hrs && mins == alm_mins && curMode != SETALARM) {
          alm_fired = true;                             // будильник звучит в текущую минуту
          alm_flag = true;                              // будильник сработал
          almTimer.reset();                             // запуск таймера предельного звучания будильника
        }
      }
    }
    if (alm_flag) {                                     // если звучит будильник
      if (almTimer.isReady()) {                         // ждём предельного времени звучания будильника
        alm_flag = false;                               // сбрасываем сработку будильника
      }
    }
  }
}
