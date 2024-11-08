/* check 28.10.20 
 *  
 */

/* Обеспечение "антиотравления"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
inline void burnIndicators() {
  for (byte k = 0; k < BURN_LOOPS; k++) {
    for (byte d = 0; d < 10; d++) {
      for (byte i = 0; i < NUMTUB; i++) {
        indiDigits[i]--;
        if (indiDigits[i] < 0) indiDigits[i] = 9;
      }
      delay((unsigned long)BURN_TIME);
    }
  }
}
