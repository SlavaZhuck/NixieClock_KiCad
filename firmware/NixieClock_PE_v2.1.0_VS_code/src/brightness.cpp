
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"

static timerMinim backlBrightTimer(30);                  // таймер шага яркости подсветки
static timerMinim dotBrightTimer(DOT_TIMER);             // таймер шага яркости точки
/* всё про точку */
static DOT_MODES dotMode;                                // текущий установленный режим работы точки
boolean dotBrightFlag, dotBrightDirection;        // индикатор времени начала отображения точки, точка по яркости возрастает/уменьшается
static byte dotMaxBright = DOT_BRIGHT;                   // максимальная яркость точки
int dotBrightCounter;                             // текущая яркость точки в процессе эффекта
static byte dotBrightStep;                               // шаг изменения яркости точки в нормальных условиях
static byte dotNumBlink;                                 // количество включений точки за период

static boolean backlBrightFlag, backlBrightDirection;
static int backlBrightCounter;
static byte backlMaxBright = BACKL_BRIGHT;
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
void dotBrightTick() {
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
