
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"
#include <EEPROM.h>

static volatile boolean dotFlag = false;              // признак фазы внутри половины секунды
static boolean alm_request = ALARM_NOREQ;             // признак необходимости проверить будильник
static boolean alm_fired = ALARM_WAIT_1;              // запрет повторного срабатывания в ту же минуту
static timerMinim almTimer((long)ALM_TIMEOUT * 1000); // таймер времени звучания будильника

/* Чтение времени из DS3231 с ожиданием стабильного значения (двух подряд
 * чтений с одинаковыми секундами) и обновление глобальных hrs/mins/secs.
 * Возвращает прочитанный DateTime — удобно вызывающей стороне.
 */
DateTime syncFromRtc()
{
  DateTime now = rtc.now();
  int8_t firstSec = now.second();
  do
  {
    now = rtc.now();
  } while (firstSec != now.second());
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();
  return now;
}

/* Применить накопленную поправку ADJUST_TIME при смене календарного месяца
 * (по данным RTC). Должна вызываться сразу после syncFromRtc(), которая уже
 * прочитала актуальное now.
 */
static void applyMonthlyDriftCorrection(DateTime now)
{
  byte currentMonth = now.month();
  if (currentMonth == lastAdjustedMonth) return;

  if (lastAdjustedMonth >= 1 && lastAdjustedMonth <= 12 && ADJUST_TIME != 0)
  {
    rtc.adjust(now + TimeSpan((int32_t)ADJUST_TIME));
    syncFromRtc();
  }
  lastAdjustedMonth = currentMonth;
  EEPROM.put(LASTADJMONTH, lastAdjustedMonth);
}

/* Ход времени, синхронизация с RTC и проверка будильника. */
void calculateTime(boolean *dotBrightFlag_local, boolean *dotBrightDirection_local, int *dotBrightCounter_local)
{
  halfsecond = false;
  dotFlag = !dotFlag;
  if (!dotFlag) return;

  *dotBrightFlag_local = true;
  *dotBrightDirection_local = true;
  *dotBrightCounter_local = 0;
  newSecFlag = true;
  secs++;

  if (secs > 59)
  {
    secs = 0;
    mins++;
    newTimeFlag = true;
    alm_request = true;
    if (mins % BURN_PERIOD == 0)
      burnIndicators();
  }
  if (mins > 59)
  {
    mins = 0;
    hrs++;
    if (hrs > 23)
      hrs = 0;
    changeBright();
    // синхронизация с RTC каждый час
    DateTime now = syncFromRtc();
    SQW_counter = 0;
    applyMonthlyDriftCorrection(now);
  }

  // каждые 10/30/50 секунд запустить автопоказ измерений
  if (auto_show_measurements && (secs == 10 || secs == 30 || secs == 50))
  {
    autoShowMeasurementsTimer.setInterval(10);
    autoShowMeasurementsTimer.reset();
  }

  if (newTimeFlag || newSecFlag)
    setNewTime(hrs, mins, secs, newTime);

  if (alm_request)
  {
    alm_request = false;
    if (alm_fired)
      alm_fired = false;
    if (alm_set && !alm_fired &&
        hrs == alm_hrs && mins == alm_mins && curMode != SETALARM)
    {
      alm_fired = true;
      alm_flag = true;
      almTimer.reset();
    }
  }
  if (alm_flag && almTimer.isReady())
    alm_flag = false;
}

/* Очистка катодов индикаторов от отравления. */
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
