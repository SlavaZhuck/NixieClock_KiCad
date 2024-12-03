/* Структурная функция основного цикла
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */


void loop() {


  if (halfsecond) calculateTime();                // каждые 500 мс пересчёт и отправка времени
  beeper();
  if (showFlag) 
  {                                 // отображение номера эффекта для цифр при переходе
    if(eshowTimer.isReady()) showFlag = false;
  } 
  else 
  {
    if ((newSecFlag || newTimeFlag) && curMode == 0) 
    {
      flipTick();     // перелистывание цифр
    }
  }
  dotBrightTick();                                // плавное мигание точки
  backlBrightTick();                              // плавное мигание подсветки ламп
  if (GLITCH_ALLOWED && curMode == SHTIME) glitchTick();  // глюки
  buttonsTick();                                  // кнопки
  settingsTick();                                 // настройки
  DCDCTick();                                     // анодное напряжение
}
