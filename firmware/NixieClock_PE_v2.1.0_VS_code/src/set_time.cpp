
#include <Arduino.h>

/* Заполнение массива отображаемых данных
 *  Входные параметры:
 *    byte hours: двузначное число, отображаемое в разрядах часов;
 *    byte minutes: двузначное число, отображаемое в разрядах минут;
 *    byte seconds: двузначное число, отображаемое в разрядах секунд.
 *  Выходные параметры: нет
 */
void sendTime(byte hours, byte minutes, byte seconds, volatile int8_t indiDigitsLocal[])
{
  indiDigitsLocal[0] = (byte)hours / 10;
  indiDigitsLocal[1] = (byte)hours % 10;

  indiDigitsLocal[2] = (byte)minutes / 10;
  indiDigitsLocal[3] = (byte)minutes % 10;

  indiDigitsLocal[4] = (byte)seconds / 10;
  indiDigitsLocal[5] = (byte)seconds % 10;
}

/* Заполнение массива новых данных для работы эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void setNewTime(byte hours, byte minutes, byte seconds, byte newTimeLocal[])
{
  newTimeLocal[0] = (byte)hours / 10;
  newTimeLocal[1] = (byte)hours % 10;

  newTimeLocal[2] = (byte)minutes / 10;
  newTimeLocal[3] = (byte)minutes % 10;
  newTimeLocal[4] = (byte)seconds / 10;
  newTimeLocal[5] = (byte)seconds % 10;
}

/* Отображение значения AUTO_ADJUST_TIME_VALUE в разрядах 0-3:
 *   разряды 0-1 — знак (00 = «+», 99 = «−»),
 *   разряды 2-3 — абсолютное значение (00-99).
 *   Разряды 4-5 гасятся.
 */
void sendAutoAdjust(int8_t val, volatile int8_t indiDigitsLocal[])
{
  byte absVal;
  if (val < 0)
  {
    indiDigitsLocal[0] = 9; // знак «−»
    indiDigitsLocal[1] = 9;
    absVal = (byte)(-val);
  }
  else
  {
    indiDigitsLocal[0] = 0; // знак «+»
    indiDigitsLocal[1] = 0;
    absVal = (byte)val;
  }
  indiDigitsLocal[2] = absVal / 10;
  indiDigitsLocal[3] = absVal % 10;
  indiDigitsLocal[4] = 0;
  indiDigitsLocal[5] = 0;
}

/* Отображение четырёхзначного года в первых четырёх разрядах
 *  (anodeStates = 0x0F).
 */
void sendYear(uint16_t year, volatile int8_t indiDigitsLocal[])
{
  indiDigitsLocal[0] = (byte)(year / 1000) % 10;
  indiDigitsLocal[1] = (byte)(year / 100) % 10;
  indiDigitsLocal[2] = (byte)(year / 10) % 10;
  indiDigitsLocal[3] = (byte)(year % 10);
  indiDigitsLocal[4] = 0;
  indiDigitsLocal[5] = 0;
}

/* Отображение пары "месяц день" в первых четырёх разрядах
 *  (anodeStates = 0x0F): MM в разрядах 0-1, DD в разрядах 2-3.
 */
void sendDate(byte month, byte day, volatile int8_t indiDigitsLocal[])
{
  indiDigitsLocal[0] = (byte)month / 10;
  indiDigitsLocal[1] = (byte)month % 10;

  indiDigitsLocal[2] = (byte)day / 10;
  indiDigitsLocal[3] = (byte)day % 10;

  indiDigitsLocal[4] = 0;
  indiDigitsLocal[5] = 0;
}