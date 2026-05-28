
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

/* Управление яркостью индикаторов, подсветки ламп и секундной точки.
 * Дневной/ночной профиль яркости переключается по часам RTC.
 */

boolean dotBrightFlag;       // фаза: точка только что начала «дышать»
boolean dotBrightDirection;  // направление изменения яркости точки
int dotBrightCounter;        // текущая яркость точки в процессе эффекта

// --- секундная точка ---
static timerMinim dotBrightTimer(DOT_TIMER);
static DOT_MODES dotMode;
static byte dotMaxBright = DOT_BRIGHT;
static byte dotBrightStep;
static byte dotNumBlink;

// --- подсветка ламп ---
static timerMinim backlBrightTimer(30);
static boolean backlBrightFlag, backlBrightDirection;
static int backlBrightCounter;
static byte backlMaxBright = BACKL_BRIGHT;

/* Подсветить одним из трёх цветов, погасив остальные. */
static void setSingleBacklightColor(byte activePin, byte brightness)
{
  for (byte i = 0; i < 3; i++)
    if (backlColors[i] != activePin)
      digitalWrite(backlColors[i], 0);
  setPWM(activePin, brightness);
}

/* Установка профиля яркости (день/ночь) и пересчёт связанных параметров. */
void changeBright()
{
#if (NIGHT_LIGHT == 1)
  boolean isNight = (hrs >= NIGHT_START && hrs <= 23) || (hrs >= 0 && hrs < NIGHT_END);
  indiMaxBright  = isNight ? INDI_BRIGHT_N  : INDI_BRIGHT;
  dotMaxBright   = isNight ? DOT_BRIGHT_N   : DOT_BRIGHT;
  backlMaxBright = isNight ? BACKL_BRIGHT_N : BACKL_BRIGHT;
#else
  indiMaxBright  = INDI_BRIGHT;
  dotMaxBright   = DOT_BRIGHT;
  backlMaxBright = BACKL_BRIGHT;
#endif

  memset((void *)indiDimm, indiMaxBright, NUMTUB);

  // режим точки зависит от состояния будильника
  dotSetMode(alm_set ? DOT_IN_ALARM : DOT_IN_TIME);

  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
  indiBrightCounter = indiMaxBright;

  // обновить подсветку, если выбран режим постоянного свечения
  if (backL_mode == 1)
    setPWM(backlColors[backlColor], backlMaxBright);
}

/* Шаг анимации «дыхания» подсветки ламп (вызывается из основного цикла). */
static void breathingTick(void)
{
  if (backlMaxBright == 0)
  {
    digitalWrite(backlColors[backlColor], 0);
    return;
  }

  if (backlBrightDirection)
  {
    // первый шаг после паузы — вернуть штатный шаг таймера
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
      // пауза между вспышками
      backlBrightTimer.setInterval(BACKL_PAUSE);
      backlBrightFlag = false;
    }
  }
  setPWM(backlColors[backlColor], getPWM_CRT(backlBrightCounter));
}

/* Реакция на смену backL_mode в режиме SHTIME. */
static void applyShtimeBacklightMode(void)
{
  digitalWrite(BACKLR, 0);
  digitalWrite(BACKLG, 0);
  digitalWrite(BACKLB, 0);
  if (backL_mode == 1)            // постоянное свечение
    setPWM(backlColors[backlColor], backlMaxBright);
  else if (backL_mode == 2)       // выключено
    digitalWrite(backlColors[backlColor], 0);
  // backL_mode == 0 — «дыхание» обрабатывается на таймере
}

/* Обновление подсветки ламп в зависимости от текущего режима интерфейса. */
void backlBrightTick()
{
  switch (curMode)
  {
  case SHTIME:
    if (chBL)
    {
      chBL = false;
      applyShtimeBacklightMode();
    }
    if (backL_mode == 0 && backlBrightTimer.isReady())
      breathingTick();
    break;

  case SETTIME:
  case SHALARM:
  case SETALARM:
    if (chBL)
    {
      chBL = false;
      digitalWrite(backlColors[backlColor], 0);
    }
    break;

  case SHTEMP: // красный
    if (chBL) { chBL = false; setSingleBacklightColor(BACKLR, getPWM_CRT(backlMaxBright / 2)); }
    break;
  case SHHUM:  // синий
    if (chBL) { chBL = false; setSingleBacklightColor(BACKLB, getPWM_CRT(backlMaxBright / 2)); }
    break;
  case SHATM:  // зелёный
    if (chBL) { chBL = false; setSingleBacklightColor(BACKLG, getPWM_CRT(backlMaxBright / 2)); }
    break;
  }
}

/* Установка режима работы секундной точки. */
void dotSetMode(DOT_MODES dMode)
{
  dotMode = dMode;
  dotBrightFlag = false;
  switch (dMode)
  {
  case DM_NULL:
    setPWM(DOT, getPWM_CRT(0));
    break;
  case DM_ONCE:
  case DM_HALF:
  case DM_TWICE:
  case DM_THREE:
    dotNumBlink = 0;
    dotBrightStep = ceil((float)dotMaxBright * 2 / DOT_TIME * DOT_TIMER);
    if (dotBrightStep == 0) dotBrightStep = 1;
    if      (dMode == DM_TWICE) dotBrightStep *= 2;
    else if (dMode == DM_THREE) dotBrightStep *= 3;
    break;
  case DM_FULL:
    setPWM(DOT, getPWM_CRT(dotMaxBright));
    break;
  }
}

/* Обеспечение работы секундной точки (вызывается из основного цикла). */
void dotBrightTick()
{
  if (dotMode == DM_NULL || dotMode == DM_FULL) return;
  if (!dotBrightFlag || !dotBrightTimer.isReady()) return;

  if (dotBrightDirection)
  {
    dotBrightCounter += dotBrightStep;
    if (dotBrightCounter >= dotMaxBright)
    {
      dotBrightDirection = false;
      dotBrightCounter = dotMaxBright;
    }
  }
  else
  {
    int minLevel = (dotMode == DM_HALF) ? dotMaxBright / 2 : 0;
    dotBrightCounter -= dotBrightStep;
    if (dotBrightCounter <= minLevel)
    {
      dotBrightDirection = true;
      if (dotMode == DM_TWICE || dotMode == DM_THREE)
      {
        dotNumBlink++;
        byte limit = (dotMode == DM_TWICE) ? 2 : 3;
        if (dotNumBlink == limit)
        {
          dotNumBlink = 0;
          dotBrightFlag = false;
        }
      }
      else
      {
        dotBrightFlag = false;
      }
      dotBrightCounter = minLevel;
    }
  }
  setPWM(DOT, getPWM_CRT(dotBrightCounter));
}
