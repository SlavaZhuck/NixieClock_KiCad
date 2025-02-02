
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

/* Обеспечение отображения эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */

/* Обеспечение хода времени и запуск зависящих от времени процессов
 *  ("антиотравления" катодов, синхронизация с RTC (в 3 часа ночи),
 *  работа будильника)
 *  
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */

static volatile boolean dotFlag = false;                 // признак фазы внутри половины секунды
static boolean alm_request = ALARM_NOREQ;                // признак необходимости проверить совпадение времени с будильником
static boolean alm_fired = ALARM_WAIT_1;                 // запрет повторного срабатывания будильника в ту же минуту
static timerMinim almTimer((long)ALM_TIMEOUT * 1000);    // таймер времени звучания будильника

void calculateTime(boolean *dotBrightFlag_local, boolean *dotBrightDirection_local, int *dotBrightCounter_local) 
{
  halfsecond = false;
  dotFlag = !dotFlag;
  if (dotFlag) 
  {
    *dotBrightFlag_local = true;
    *dotBrightDirection_local = true;
    *dotBrightCounter_local = 0;
    newSecFlag = true;
    secs++;
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
      // if (hrs == 3) {                                   // синхронизация с RTC в 3 часа ночи
      {                                                    // синхронизация с RTC каждый час
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
        SQW_counter = 0;
        mins = now.minute();
        hrs = now.hour();
      }
    }


    if (auto_show_measurements)
    {
      if (secs == 10 || secs == 30 || secs == 50) // каждые 10, 30 и 50 секунд запускается таймер на автопоказ измерений
      {      
        autoShowMeasurementsTimer.setInterval(10);
        autoShowMeasurementsTimer.reset();
      }
    }

    if (newTimeFlag || newSecFlag) setNewTime(hrs, mins, secs, newTime);        // обновляем массив времени

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



/* check 28.10.20
 *
 */

/* Обеспечение "антиотравления"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void burnIndicators()
{
  for (byte k = 0; k < BURN_LOOPS; k++)
  {
    for (byte d = 0; d < 10; d++)
    {
      for (byte i = 0; i < NUMTUB; i++)
      {
        indiDigits[i]--;
        if (indiDigits[i] < 0)
          indiDigits[i] = 9;
      }
      delay((unsigned long)BURN_TIME);
    }
  }
}