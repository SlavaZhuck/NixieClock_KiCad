/* Поведение отображаемых значений в режимах установки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void settingsTick() {
  if (curMode == SETTIME || curMode == SETALARM) {
    if (curMode == SETALARM && !alm_set) {          // мигать отображением времени будильника, если будильник не установлен
      if (!(anodeStates == 0 || anodeStates == 0xF)) anodeStates = 0;
      if (blinkTimer.isReady()) anodeStates ^= 0xF;
    } else {
      if (blinkTimer.isReady()) {
        lampState = !lampState;
        if (lampState) anodeStates = 0xF;
        else if (!currentDigit) anodeStates = 0x0C;
        else anodeStates = 0x3; 
      }
    }
  }
}

/* Возврат к отображению времени
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void retToTime() {
  curMode = SHTIME;

#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
  anodeStates = 0x3F;
  sendTime(hrs, mins, secs);
#else
  anodeStates = 0xF;
  sendTime(hrs, mins);
#endif

  dotSetMode( alm_set ? DOT_IN_ALARM : DOT_IN_TIME );
  chBL = true;
}

/* Обработка нажатий кнопок
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void buttonsTick() {

  btnA.tick();                                    // определение, нажата ли кнопка Alarm
  // сначала - особый вариант реагирования на кнопки в режиме сработавшего будильника
  if (alm_flag) {
    if (btnA.isClick() || btnA.isHolded()) alm_flag = false;
    return;
  }

  int analog = analogRead(A7);                    // чтение нажатой кнопки
  btnSet.tick(analog <= 1023 && analog > 950);    // определение, нажата ли кнопка Set
  btnL.tick(analog <= 860 && analog > 450);       // определение, нажата ли кнопка Up
  btnR.tick(analog <= 380 && analog > 100);       // определение, нажата ли кнопка Down

  switch (curMode) {
    /*------------------------------------------------------------------------------------------------------------------------------*/
    case SHTIME:                                  // (0) отображение часов
      if (btnR.isClick()) {                       // переключение эффектов цифр
        if (++FLIP_EFFECT >= FLIP_EFFECT_NUM) FLIP_EFFECT = FM_NULL;
        EEPROM.put(FLIPEFF, FLIP_EFFECT);
                                                  // для показа номера эффекта
        eshowTimer.reset();
        showFlag = true;
        memset(indiDimm, indiMaxBright, NUMTUB);
        memset(indiDigits, FLIP_EFFECT, NUMTUB);

#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
        anodeStates = 0x3F;
        newSecFlag = true;
#else
        anodeStates = 0xF;
#endif 
        newTimeFlag = true;
      }
    
      if (btnL.isClick()) {                       // переключение эффектов подсветки
        if (++BACKL_MODE >= 3) {
          BACKL_MODE = 0;
          digitalWrite(backlColors[backlColor], 0);
          if(++backlColor >= 3) backlColor = 0;
          EEPROM.put(BLCOLOR, backlColor);
        }
        EEPROM.put(LIGHTEFF, BACKL_MODE);
        chBL = true;
      }

      if (btnL.isHolded()) {                      // переключение глюков
        GLITCH_ALLOWED = !GLITCH_ALLOWED;
        EEPROM.put(GLEFF, GLITCH_ALLOWED);
      }

      if (btnA.isClick() || btnA.isHolded()) {    // переход в режим отображения температуры
        curMode = SHTEMP;
        if (isBMEhere) { 
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          indiDigits[0] = (byte)((int)temp_event.temperature / 10);
          indiDigits[1] = (byte)((int)temp_event.temperature % 10);
          indiDigits[2] = (byte)((int)(temp_event.temperature * 10) % 10); 
        } else {
          indiDigits[0] = (byte) 0;
          indiDigits[1] = (byte) 0;
          indiDigits[2] = (byte) 0; 
        }
        measurementsTimer.reset();
        anodeStates = 0x07;
        autoTimer.setInterval(TEMP_SH_TIME);
        autoTimer.reset();
        dotSetMode( DM_FULL );
        chBL = true;
      }
      
      if (btnSet.isDouble()) {                    // переход в режим установки времени
        anodeStates = 0x0F;
        currentDigit = false;
        curMode = SETTIME;
        changeHrs = hrs;
        changeMins = mins;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
        sendTime(changeHrs, changeMins, 0);
#else
        sendTime(changeHrs, changeMins);
#endif

        chBL = true;
      }

      if (btnSet.isHolded()) {                   // переход в режим установки будильника и времени его
        anodeStates = 0x0F;
        currentDigit = false;
        curMode = SETALARM;
        changeHrs = alm_hrs;
        changeMins = alm_mins;
        
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
        sendTime(changeHrs, changeMins, 0);
#else
        sendTime(changeHrs, changeMins);
#endif

        dotSetMode( DM_NULL );
        chBL = true;
      }

      break;
      
    /*------------------------------------------------------------------------------------------------------------------------------*/
    case SETTIME:                                 // (1) установка часов
    case SETALARM:                                // (3) установка времени будильника

      if (!(curMode == SETALARM && !alm_set)) {
                                                  // переход между разрядами      
        if (btnSet.isClick()) currentDigit = !currentDigit;
        if (btnSet.isHolded()) {                  // обнуление текущего рвазряда
          if (!currentDigit) changeHrs = 0;
          else changeMins = 0;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(changeHrs, changeMins, 0);
#else
          sendTime(changeHrs, changeMins);
#endif

        }

        if (btnL.isClick()) {                     // уменьшить значение
          if (!currentDigit) {
            changeHrs--;
            if (changeHrs < 0) changeHrs = 23;
          } else {
            changeMins--;
            if (changeMins < 0) {
              changeMins = 59;
              changeHrs--;
              if (changeHrs < 0) changeHrs = 23;
            }
          }
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(changeHrs, changeMins, 0);
#else
          sendTime(changeHrs, changeMins);
#endif
        }

        if (btnL.isHolded()) {                     // уменьшить значение на 5
          if (!currentDigit) {
            changeHrs -= 5;
            if (changeHrs < 0) changeHrs += 24;
          } else {
            changeMins -= 5;
            if (changeMins < 0) {
              changeMins += 60;
              changeHrs--;
              if (changeHrs < 0) changeHrs = 23;
            }
          }
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(changeHrs, changeMins, 0);
#else
          sendTime(changeHrs, changeMins);
#endif
        }
      
        if (btnR.isClick()) {                     // увеличить значение
          if (!currentDigit) {
            changeHrs++;
            if (changeHrs > 23) changeHrs = 0;
          } else {
            changeMins++;
            if (changeMins > 59) {
              changeMins = 0;
              changeHrs++;
              if (changeHrs > 23) changeHrs = 0;
            }
          }
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(changeHrs, changeMins, 0);
#else
          sendTime(changeHrs, changeMins);
#endif
        }
      
        if (btnR.isClick()) {                     // увеличить значение на 5
          if (!currentDigit) {
            changeHrs += 5;
            if (changeHrs > 23) changeHrs -= 24;
          } else {
            changeMins += 5;
            if (changeMins > 59) {
              changeMins -= 60;
              changeHrs++;
              if (changeHrs > 23) changeHrs = 0;
            }
          }
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(changeHrs, changeMins, 0);
#else
          sendTime(changeHrs, changeMins);
#endif
        }
      }
                                                  
      if (btnA.isHolded()) {                      // переход в режим отображения времени без сохранения или смена установки будильника
        if (curMode == SETTIME)  retToTime();
        else alm_set = !alm_set;
      }

      if (btnA.isClick()) {
        if (curMode == SETTIME) 
        {
          hrs = changeHrs;
          mins = changeMins;
          secs = 0;
          DateTime now = rtc.now();
          rtc.adjust(DateTime(now.year(), now.month(), now.day(), hrs, mins, 0));
          SQW_counter = 0;
          changeBright(); 
        } else {
          alm_hrs = changeHrs;
          alm_mins = changeMins;
          EEPROM.put(ALHOUR, alm_hrs);
          EEPROM.put(ALMIN, alm_mins);
          EEPROM.put(ALIFSET, alm_set);
        }
        retToTime();
      }

      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHALARM:                                 // (2) отображение времени будильника (5 сек)
      if (autoTimer.isReady() || btnA.isClick() || btnA.isHolded()) retToTime();
      
      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHTEMP:                                  // (4) отображение температуры (5 сек)
      if (measurementsTimer.isReady()) {
        if (isBMEhere) { 
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          indiDigits[0] = (byte)((int)temp_event.temperature / 10);
          indiDigits[1] = (byte)((int)temp_event.temperature % 10);
          indiDigits[2] = (byte)((int)(temp_event.temperature * 10) % 10); 
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) {
        curMode = SHATM;
        if (isBMEhere) {
          float pressure_in_mm = pressure_event.pressure / 1.333223684;
          indiDigits[1] = (byte)((int)pressure_in_mm / 100);
          indiDigits[2] = (byte)(((int)pressure_in_mm / 10) % 10);
          indiDigits[3] = (byte)((int)pressure_in_mm % 10);
        } else {
          indiDigits[1] = (byte)0;
          indiDigits[2] = (byte)0;
          indiDigits[3] = (byte)0;
        }
        anodeStates = 0x0E;
        dotSetMode( DM_NULL );
        autoTimer.setInterval(ATMOSPHERE_SH_TIME);
        autoTimer.reset();
        chBL = true;
      }
      
      break;

    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHHUM:                                   // (5) отображение влажности (5 сек)
      if (measurementsTimer.isReady()) {
        if (isBMEhere) {
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          if ((int)humidity_event.relative_humidity == 100) indiDigits[4] = indiDigits[5] = 9;
          else {
            indiDigits[4] = (byte)((int)humidity_event.relative_humidity / 10);
            indiDigits[5] = (byte)((int)humidity_event.relative_humidity % 10);
          }
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) {
        if (alm_set) {
          curMode = SHALARM;
          anodeStates = 0x0F;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          sendTime(alm_hrs, alm_mins, 0);
#else
          sendTime(alm_hrs, alm_mins);
#endif
          autoTimer.setInterval(ALARM_SH_TIME);
          autoTimer.reset();
          dotSetMode( DM_NULL );
          chBL = true;
        } else retToTime();
      }
      break;
      
    /*------------------------------------------------------------------------------------------------------------------------------*/ 
    case SHATM:                                   // (6) отображение атмосферного давления (5 сек)
      if (measurementsTimer.isReady()) {
        if (isBMEhere) {
          bme_temp->getEvent(&temp_event);
          bme_pressure->getEvent(&pressure_event);
          bme_humidity->getEvent(&humidity_event);
          float pressure_in_mm = pressure_event.pressure / 1.333223684;
          indiDigits[1] = (byte)((int)pressure_in_mm / 100);
          indiDigits[2] = (byte)(((int)pressure_in_mm / 10) % 10);
          indiDigits[3] = (byte)((int)pressure_in_mm % 10);
        }
      }
      if (btnA.isHolded()) retToTime();
      if (btnSet.isHolded()) isFreeze = !isFreeze;
      if ( (autoTimer.isReady() || btnA.isClick()) && !isFreeze) {
        curMode = SHHUM;
        if (isBMEhere) {
          if ((int)humidity_event.relative_humidity == 100) indiDigits[4] = indiDigits[5] = 9;
          else {
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
            indiDigits[4] = (byte)((int)humidity_event.relative_humidity / 10);
            indiDigits[5] = (byte)((int)humidity_event.relative_humidity % 10);
#else
            indiDigits[2] = (byte)((int)humidity_event.relative_humidity / 10);
            indiDigits[3] = (byte)((int)humidity_event.relative_humidity % 10);
#endif
          }
        } else {
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
          indiDigits[4] = indiDigits[5] = 0;
#else
          indiDigits[2] = indiDigits[3] = 0;
#endif
        }
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
        anodeStates = 0x30;
#else
        anodeStates = 0xC;
#endif
        autoTimer.setInterval(HUMIDITY_SH_TIME);
        autoTimer.reset();
        chBL = true;
      }
      
      break;
  }
}
