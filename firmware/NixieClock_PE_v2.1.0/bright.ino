/* Обеспечение работы подсветки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void backlBrightTick() {
  switch (curMode) {
    case SHTIME:                                  // (0) отображение часов
      if (chBL) {
        chBL = false;
        digitalWrite(BACKLR, 0);
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLB, 0);
        if (BACKL_MODE == 1) {
          setPWM(backlColors[backlColor], backlMaxBright);
        } else if (BACKL_MODE == 2) {
          digitalWrite(backlColors[backlColor], 0);
        }
      }
      if (BACKL_MODE == 0 && backlBrightTimer.isReady()) {
        if (backlMaxBright > 0) {
          if (backlBrightDirection) {
            if (!backlBrightFlag) {
              backlBrightFlag = true;
              backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
            }
            backlBrightCounter += BACKL_STEP;
            if (backlBrightCounter >= backlMaxBright) {
              backlBrightDirection = false;
              backlBrightCounter = backlMaxBright;
            }
          } else {
            backlBrightCounter -= BACKL_STEP;
            if (backlBrightCounter <= BACKL_MIN_BRIGHT) {
              backlBrightDirection = true;
              backlBrightCounter = BACKL_MIN_BRIGHT;
              backlBrightTimer.setInterval(BACKL_PAUSE);
              backlBrightFlag = false;
            }
          }
          setPWM(backlColors[backlColor], getPWM_CRT(backlBrightCounter));
        } else {
          digitalWrite(backlColors[backlColor], 0);
        }
      }
      break;
      
    case SETTIME:                                 // (1) установка часов
    case SHALARM:                                 // (2) отображение времени будильника (5 сек)
    case SETALARM:                                // (3) установка времени будильника
      if (chBL) {
        chBL = false;
        digitalWrite(backlColors[backlColor], 0);
      }
      break;
    
    case SHTEMP:                                  // (4) отображение температуры (5 сек)
      if (chBL) {
        chBL = false;
        setPWM(BACKLR, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLB, 0);
      }
      break;
    case SHHUM:                                   // (5) отображение влажности (5 сек)
      if (chBL) {
        chBL = false;
        setPWM(BACKLB, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLG, 0);
        digitalWrite(BACKLR, 0);
      }
      break;
    case SHATM:                                   // (6) отображение атмосферного давления (5 сек)
      if (chBL) {
        chBL = false;
        setPWM(BACKLG, getPWM_CRT(backlMaxBright / 2));
        digitalWrite(BACKLR, 0);
        digitalWrite(BACKLB, 0);
      }
      break;
  }
}

/* Обеспечение смены яркости от времени суток
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void changeBright() {
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

  memset(indiDimm, indiMaxBright, NUMTUB);

  // установить режим работы секундной точки в зависимости от установок будильника
  dotSetMode( alm_set ? DOT_IN_ALARM : DOT_IN_TIME );

  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
  indiBrightCounter = indiMaxBright;

  //change PWM to apply backlMaxBright in case of maximum bright mode
  if (BACKL_MODE == 1) setPWM(backlColors[backlColor], backlMaxBright);
}
