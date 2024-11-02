/* Структурная функция основного цикла
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
unsigned timestamp = 0;
unsigned timestamp_half_sec = 0;


void loop() {
  
  if ( uint16_t((micros()) - timestamp) >= 100)
  {
    RTC_handler();
  }

  if (uint16_t(millis()) - timestamp_half_sec >= 499)
  {
    timestamp_half_sec = uint16_t(millis());
    halfsecond = true;
  }


  if (halfsecond) calculateTime();                // каждые 500 мс пересчёт и отправка времени
  beeper();
  if (showFlag) 
  {                                 // отображение номера эффекта для цифр при переходе
    if(eshowTimer.isReady()) showFlag = false;
  } else 
  {
    if ((newSecFlag || newTimeFlag) && curMode == 0) flipTick();     // перелистывание цифр
  }
  dotBrightTick();                                // плавное мигание точки
  backlBrightTick();                              // плавное мигание подсветки ламп
  if (GLITCH_ALLOWED && curMode == SHTIME) glitchTick();  // глюки
  buttonsTick();                                  // кнопки
  settingsTick();                                 // настройки
  DCDCTick();                                     // анодное напряжение
}
