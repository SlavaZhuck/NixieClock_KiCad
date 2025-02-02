
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