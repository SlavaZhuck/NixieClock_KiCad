/* check 28.10.20 
 *  
*/

/* Заполнение массива отображаемых данных
 *  Входные параметры:
 *    byte hours: двузначное число, отображаемое в разрядах часов;
 *    byte minutes: двузначное число, отображаемое в разрядах минут;
 *    byte seconds: двузначное число, отображаемое в разрядах секунд.
 *  Выходные параметры: нет
 */
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
void sendTime(byte hours, byte minutes, byte seconds) {
  indiDigits[0] = (byte)hours / 10;
  indiDigits[1] = (byte)hours % 10;

  indiDigits[2] = (byte)minutes / 10;
  indiDigits[3] = (byte)minutes % 10;

  indiDigits[4] = (byte)seconds / 10;
  indiDigits[5] = (byte)seconds % 10;
}
#else
void sendTime(byte hours, byte minutes) {
  indiDigits[0] = (byte)hours / 10;
  indiDigits[1] = (byte)hours % 10;

  indiDigits[2] = (byte)minutes / 10;
  indiDigits[3] = (byte)minutes % 10;
}
#endif

/* Заполнение массива новых данных для работы эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void setNewTime() {
  newTime[0] = (byte)hrs / 10;
  newTime[1] = (byte)hrs % 10;

  newTime[2] = (byte)mins / 10;
  newTime[3] = (byte)mins % 10;
#if (BOARD_TYPE == 0) || (BOARD_TYPE == 1) || (BOARD_TYPE == 2) || (BOARD_TYPE == 3)
  newTime[4] = (byte)secs / 10;
  newTime[5] = (byte)secs % 10;  
#endif
}
