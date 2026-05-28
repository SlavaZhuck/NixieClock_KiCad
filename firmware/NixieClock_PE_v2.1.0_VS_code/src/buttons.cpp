
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"

#include <EEPROM.h>

// SETTIME проходит последовательно по 5 разрядам, SETALARM использует
// прежний булев флаг (часы/минуты).
enum SET_STAGE : byte
{
  SET_ADJUST, // (0) значение AUTO_ADJUST_TIME_VALUE (перед годом)
  SET_YEAR,
  SET_MONTH,
  SET_DAY,
  SET_HRS,
  SET_MIN,
  SET_STAGE_NUM
};

static boolean currentDigit = false;      // для SETALARM: false=часы, true=минуты
static SET_STAGE setTimeStage = SET_ADJUST; // для SETTIME
static int8_t changeAutoAdjust;           // редактируемое значение AUTO_ADJUST_TIME_VALUE

static sensors_event_t temp_event, pressure_event, humidity_event;
static boolean isFreeze = false;
static int8_t changeHrs, changeMins;
static uint16_t changeYear;
static int8_t changeMonth, changeDay;
static timerMinim blinkTimer(500); // таймер моргания
static boolean lampState = false;

static void retToTime(boolean *chBL_local);
static void settingsTick(void);
static byte daysInMonth(byte month, uint16_t year);
static void refreshSetTimeDisplay(void);
static void adjustSetTimeStage(int delta);
static void resetSetTimeStage(void);
static void bumpAlarm(int delta, boolean editingMins);
static void readBme(void);
static void showTemperature(void);
static void showPressure(void);
static void showHumidity(void);
static void enterSetTime(boolean *chBL_local);
static void enterSetAlarm(boolean *chBL_local);
static void enterShowTemp(boolean *chBL_local);
static void enterShowPressure(boolean *chBL_local);
static void enterShowHumidity(boolean *chBL_local);
static void enterShowAlarm(boolean *chBL_local);

/* Обработка нажатий кнопок */
void buttonsTick(boolean *showFlag_local, volatile unsigned int *SQW_counter_local, boolean *chBL_local)
{
  btnA.tick();
  // в режиме сработавшего будильника любая активность сбрасывает сигнал
  if (alm_flag)
  {
    if (btnA.isClick() || btnA.isHolded())
      alm_flag = false;
    return;
  }

  int analog = analogRead(A7);
  btnSet.tick(analog <= 1023 && analog > 950);
  btnL.tick(analog <= 860 && analog > 450);
  btnR.tick(analog <= 380 && analog > 100);

  switch (curMode)
  {
  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SHTIME: // (0) отображение часов
    if (btnR.isClick()) // переключение эффектов цифр
    {
      if (++flip_effect >= flip_effect_num)
        flip_effect = FM_NULL;
      EEPROM.put(FLIPEFF, flip_effect);
      eshowTimer.reset();
      *showFlag_local = true;
      memset((void *)indiDimm, indiMaxBright, NUMTUB);
      memset((void *)indiDigits, flip_effect, NUMTUB);

      anodeStates = 0x3F;
      newSecFlag = true;
      newTimeFlag = true;
    }

    if (btnR.isHolded()) // автопоказ температуры/давления/влажности
    {
      auto_show_measurements = !auto_show_measurements;
      EEPROM.put(AUTOSHOWMEAS, auto_show_measurements);
    }

    if (btnL.isClick()) // переключение эффектов подсветки
    {
      if (++backL_mode >= 3)
      {
        backL_mode = 0;
        digitalWrite(backlColors[backlColor], 0);
        if (++backlColor >= 3)
          backlColor = 0;
        EEPROM.put(BLCOLOR, backlColor);
      }
      EEPROM.put(LIGHTEFF, backL_mode);
      *chBL_local = true;
    }

    if (btnL.isHolded()) // переключение глюков
    {
      glitch_allowed = !glitch_allowed;
      EEPROM.put(GLEFF, glitch_allowed);
    }

    if (btnA.isClick() || btnA.isHolded() || autoShowMeasurementsTimer.isReadyDisable())
      enterShowTemp(chBL_local);

    if (btnSet.isDouble()) // переход в режим установки времени
      enterSetTime(chBL_local);

    if (btnSet.isHolded()) // переход в режим установки будильника
      enterSetAlarm(chBL_local);

    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SETTIME: // (1) установка часов и даты
    if (btnSet.isClick()) // следующий разряд: год -> месяц -> день -> часы -> минуты -> год ...
    {
      setTimeStage = (SET_STAGE)((setTimeStage + 1) % SET_STAGE_NUM);
      refreshSetTimeDisplay();
    }
    if (btnSet.isHolded()) // сброс текущего разряда к значению по умолчанию
      resetSetTimeStage();

    if (btnL.isClick())   adjustSetTimeStage(-1);
    if (btnL.isHolded())  adjustSetTimeStage(-5);
    if (btnR.isClick())   adjustSetTimeStage(+1);
    if (btnR.isHolded())  adjustSetTimeStage(+5);

    if (btnA.isHolded()) // выход без сохранения
    {
      retToTime(chBL_local);
    }
    else if (btnA.isClick()) // сохранение
    {
      autoAdjustTimeValue = changeAutoAdjust;
      EEPROM.put(AUTOADJVAL, autoAdjustTimeValue);
      hrs = changeHrs;
      mins = changeMins;
      secs = 0;
      rtc.adjust(DateTime(changeYear, changeMonth, changeDay, hrs, mins, 0));
      *SQW_counter_local = 0;
      // пользователь только что выставил дату — не применять коррекцию
      // в текущем месяце повторно
      lastAdjustedMonth = changeMonth;
      EEPROM.put(LASTADJMONTH, lastAdjustedMonth);
      changeBright();
      retToTime(chBL_local);
    }
    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SETALARM: // (3) установка времени будильника
    if (alm_set)
    {
      if (btnSet.isClick())
        currentDigit = !currentDigit;
      if (btnSet.isHolded()) // обнуление текущего разряда
      {
        if (!currentDigit) changeHrs = 0;
        else               changeMins = 0;
        sendTime(changeHrs, changeMins, 0, indiDigits);
      }

      if (btnL.isClick())   bumpAlarm(-1, currentDigit);
      if (btnL.isHolded())  bumpAlarm(-5, currentDigit);
      if (btnR.isClick())   bumpAlarm(+1, currentDigit);
      if (btnR.isHolded())  bumpAlarm(+5, currentDigit);
    }

    if (btnA.isHolded()) // включение/выключение будильника
      alm_set = !alm_set;

    if (btnA.isClick())
    {
      alm_hrs = changeHrs;
      alm_mins = changeMins;
      EEPROM.put(ALHOUR, alm_hrs);
      EEPROM.put(ALMIN, alm_mins);
      EEPROM.put(ALIFSET, alm_set);
      retToTime(chBL_local);
    }
    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SHALARM: // (2) отображение времени будильника (5 сек)
    if (autoTimer.isReady() || btnA.isClick() || btnA.isHolded())
      retToTime(chBL_local);
    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SHTEMP: // (4) отображение температуры
    if (measurementsTimer.isReady())
    {
      readBme();
      showTemperature();
    }
    if (btnA.isHolded())
      retToTime(chBL_local);
    if (btnSet.isHolded())
      isFreeze = !isFreeze;
    if ((autoTimer.isReady() || btnA.isClick()) && !isFreeze)
      enterShowPressure(chBL_local);
    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SHATM: // (6) отображение атмосферного давления
    if (measurementsTimer.isReady())
    {
      readBme();
      showPressure();
    }
    if (btnA.isHolded())
      retToTime(chBL_local);
    if (btnSet.isHolded())
      isFreeze = !isFreeze;
    if ((autoTimer.isReady() || btnA.isClick()) && !isFreeze)
      enterShowHumidity(chBL_local);
    break;

  /*------------------------------------------------------------------------------------------------------------------------------*/
  case SHHUM: // (5) отображение влажности
    if (measurementsTimer.isReady())
    {
      readBme();
      showHumidity();
    }
    if (btnA.isHolded())
      retToTime(chBL_local);
    if (btnSet.isHolded())
      isFreeze = !isFreeze;
    if ((autoTimer.isReady() || btnA.isClick()) && !isFreeze)
    {
      if (alm_set)
        enterShowAlarm(chBL_local);
      else
        retToTime(chBL_local);
    }
    break;
  }

  settingsTick();
}

/* Возврат к отображению времени */
void retToTime(boolean *chBL_local)
{
  curMode = SHTIME;
  anodeStates = 0x3F;
  sendTime(hrs, mins, secs, indiDigits);
  dotSetMode(alm_set ? DOT_IN_ALARM : DOT_IN_TIME);
  *chBL_local = true;
}

/* Моргание разрядов в режимах установки времени и будильника */
static void settingsTick()
{
  if (curMode == SETTIME)
  {
    if (blinkTimer.isReady())
    {
      lampState = !lampState;
      if (lampState)
      {
        anodeStates = 0xF;
      }
      else
      {
        // карта моргающих разрядов по стадии
        switch (setTimeStage)
        {
        case SET_ADJUST:
        case SET_YEAR:                anodeStates = 0x0; break;  // моргают все 4
        case SET_MONTH: case SET_HRS: anodeStates = 0x0C; break; // моргают левые 2
        case SET_DAY:   case SET_MIN: anodeStates = 0x03; break; // моргают правые 2
        default: break;
        }
      }
    }
  }
  else if (curMode == SETALARM)
  {
    if (!alm_set)
    { // моргать всеми разрядами, если будильник выключен
      if (!(anodeStates == 0 || anodeStates == 0xF))
        anodeStates = 0;
      if (blinkTimer.isReady())
        anodeStates ^= 0xF;
    }
    else if (blinkTimer.isReady())
    {
      lampState = !lampState;
      if (lampState)         anodeStates = 0xF;
      else if (!currentDigit) anodeStates = 0x0C;
      else                    anodeStates = 0x03;
    }
  }
}

/* Количество дней в месяце с учётом високосного года (григорианский). */
static byte daysInMonth(byte month, uint16_t year)
{
  static const byte dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 31;
  if (month == 2)
  {
    boolean leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return dim[month - 1];
}

/* Перерисовка индикаторов в соответствии с текущей стадией SETTIME. */
static void refreshSetTimeDisplay()
{
  switch (setTimeStage)
  {
  case SET_ADJUST:
    sendAutoAdjust(changeAutoAdjust, indiDigits);
    break;
  case SET_YEAR:
    sendYear(changeYear, indiDigits);
    break;
  case SET_MONTH:
  case SET_DAY:
    sendDate((byte)changeMonth, (byte)changeDay, indiDigits);
    break;
  case SET_HRS:
  case SET_MIN:
  default:
    sendTime((byte)changeHrs, (byte)changeMins, 0, indiDigits);
    break;
  }
}

/* Прибавить delta (может быть отрицательной) к текущему разряду SETTIME
 * с правильной круговой обёрткой и подгонкой дня под месяц/год.
 */
static void adjustSetTimeStage(int delta)
{
  switch (setTimeStage)
  {
  case SET_ADJUST:
  {
    int v = (int)changeAutoAdjust + delta;
    if (v > 99)  v = 99;
    if (v < -99) v = -99;
    changeAutoAdjust = (int8_t)v;
    break;
  }
  case SET_YEAR:
  {
    int y = (int)changeYear + delta;
    while (y < 2000) y += 100;
    while (y > 2099) y -= 100;
    changeYear = (uint16_t)y;
    break;
  }
  case SET_MONTH:
  {
    int m = ((int)changeMonth - 1 + delta) % 12;
    if (m < 0) m += 12;
    changeMonth = (int8_t)(m + 1);
    break;
  }
  case SET_DAY:
  {
    byte dim = daysInMonth(changeMonth, changeYear);
    int d = ((int)changeDay - 1 + delta) % (int)dim;
    if (d < 0) d += dim;
    changeDay = (int8_t)(d + 1);
    break;
  }
  case SET_HRS:
  {
    int h = ((int)changeHrs + delta) % 24;
    if (h < 0) h += 24;
    changeHrs = (int8_t)h;
    break;
  }
  case SET_MIN:
  {
    int v = ((int)changeMins + delta) % 60;
    if (v < 0) v += 60;
    changeMins = (int8_t)v;
    break;
  }
  default: break;
  }
  // после изменения месяца/года день мог выйти за допустимый диапазон
  if (setTimeStage == SET_YEAR || setTimeStage == SET_MONTH)
  {
    byte dim = daysInMonth(changeMonth, changeYear);
    if (changeDay > dim) changeDay = dim;
  }
  refreshSetTimeDisplay();
}

/* Сбросить текущий разряд SETTIME к значению по умолчанию. */
static void resetSetTimeStage()
{
  switch (setTimeStage)
  {
  case SET_ADJUST: changeAutoAdjust = 0; break;
  case SET_YEAR:  changeYear = 2026; break;
  case SET_MONTH: changeMonth = 1; break;
  case SET_DAY:   changeDay = 1; break;
  case SET_HRS:   changeHrs = 0; break;
  case SET_MIN:   changeMins = 0; break;
  default: break;
  }
  if (setTimeStage == SET_YEAR || setTimeStage == SET_MONTH)
  {
    byte dim = daysInMonth(changeMonth, changeYear);
    if (changeDay > dim) changeDay = dim;
  }
  refreshSetTimeDisplay();
}

/* Прибавить delta к выбранному разряду будильника (часы/минуты)
 * с переносом из минут в часы при переполнении.
 */
static void bumpAlarm(int delta, boolean editingMins)
{
  if (!editingMins)
  {
    int h = ((int)changeHrs + delta) % 24;
    if (h < 0) h += 24;
    changeHrs = (int8_t)h;
  }
  else
  {
    int v = (int)changeMins + delta;
    while (v < 0)   { v += 60; changeHrs = (changeHrs + 23) % 24; }
    while (v >= 60) { v -= 60; changeHrs = (changeHrs + 1)  % 24; }
    changeMins = (int8_t)v;
  }
  sendTime(changeHrs, changeMins, 0, indiDigits);
}

/* Прочитать текущие значения из BME280 (если он есть). */
static void readBme()
{
  if (!isBMEhere) return;
  bme_temp->getEvent(&temp_event);
  bme_pressure->getEvent(&pressure_event);
  bme_humidity->getEvent(&humidity_event);
}

/* Заполнить разряды 0..2 значением температуры (XX.X). */
static void showTemperature()
{
  if (isBMEhere)
  {
    int t = (int)temp_event.temperature;
    indiDigits[0] = (byte)(t / 10);
    indiDigits[1] = (byte)(t % 10);
    indiDigits[2] = (byte)((int)(temp_event.temperature * 10) % 10);
  }
  else
  {
    indiDigits[0] = indiDigits[1] = indiDigits[2] = 0;
  }
}

/* Заполнить разряды 1..3 значением давления в мм рт.ст. (XXX). */
static void showPressure()
{
  if (isBMEhere)
  {
    int p = (int)(pressure_event.pressure / 1.333223684f);
    indiDigits[1] = (byte)(p / 100);
    indiDigits[2] = (byte)((p / 10) % 10);
    indiDigits[3] = (byte)(p % 10);
  }
  else
  {
    indiDigits[1] = indiDigits[2] = indiDigits[3] = 0;
  }
}

/* Заполнить разряды 4..5 значением относительной влажности (XX).
 * 100% отображается как «99».
 */
static void showHumidity()
{
  if (isBMEhere)
  {
    int h = (int)humidity_event.relative_humidity;
    if (h == 100)
    {
      indiDigits[4] = indiDigits[5] = 9;
    }
    else
    {
      indiDigits[4] = (byte)(h / 10);
      indiDigits[5] = (byte)(h % 10);
    }
  }
  else
  {
    indiDigits[4] = indiDigits[5] = 0;
  }
}

/* Вход в режим установки времени. */
static void enterSetTime(boolean *chBL_local)
{
  anodeStates = 0x0F;
  setTimeStage = SET_ADJUST;
  curMode = SETTIME;
  changeAutoAdjust = autoAdjustTimeValue;
  DateTime now = rtc.now();
  changeYear = now.year();
  changeMonth = now.month();
  changeDay = now.day();
  changeHrs = hrs;
  changeMins = mins;
  refreshSetTimeDisplay();
  *chBL_local = true;
}

/* Вход в режим установки будильника. */
static void enterSetAlarm(boolean *chBL_local)
{
  anodeStates = 0x0F;
  currentDigit = false;
  curMode = SETALARM;
  changeHrs = alm_hrs;
  changeMins = alm_mins;
  sendTime(changeHrs, changeMins, 0, indiDigits);
  dotSetMode(DM_NULL);
  *chBL_local = true;
}

/* Вход в режим показа температуры (из SHTIME). */
static void enterShowTemp(boolean *chBL_local)
{
  curMode = SHTEMP;
  readBme();
  showTemperature();
  measurementsTimer.reset();
  anodeStates = 0x07;
  autoTimer.setInterval(TEMP_SH_TIME);
  autoTimer.reset();
  dotSetMode(DM_FULL);
  *chBL_local = false;
}

/* Переход к показу давления (из SHTEMP). */
static void enterShowPressure(boolean *chBL_local)
{
  curMode = SHATM;
  showPressure();
  anodeStates = 0x0E;
  dotSetMode(DM_NULL);
  autoTimer.setInterval(ATMOSPHERE_SH_TIME);
  autoTimer.reset();
  *chBL_local = false;
}

/* Переход к показу влажности (из SHATM). */
static void enterShowHumidity(boolean *chBL_local)
{
  curMode = SHHUM;
  showHumidity();
  anodeStates = 0x30;
  autoTimer.setInterval(HUMIDITY_SH_TIME);
  autoTimer.reset();
  *chBL_local = false;
}

/* Переход к показу времени будильника (из SHHUM, если будильник включён). */
static void enterShowAlarm(boolean *chBL_local)
{
  curMode = SHALARM;
  anodeStates = 0x0F;
  sendTime(alm_hrs, alm_mins, 0, indiDigits);
  autoTimer.setInterval(ALARM_SH_TIME);
  autoTimer.reset();
  dotSetMode(DM_FULL);
  *chBL_local = true;
}
