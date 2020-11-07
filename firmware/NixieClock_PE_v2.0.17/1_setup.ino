/* check 01.11.20
 *  
 */


/* Структурная функция начальной установки
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void setup() {
  // случайное зерно для генератора случайных чисел
  randomSeed(analogRead(6) + analogRead(7));
  
  anodeStates = 0x3F;

  // настройка пинов на вход
  pinMode(ALARM_STOP, INPUT);

  // настройка пинов на выход
  pinMode(DECODER0, OUTPUT);
  pinMode(DECODER1, OUTPUT);
  pinMode(DECODER2, OUTPUT);
  pinMode(DECODER3, OUTPUT);
  pinMode(KEY0, OUTPUT);
  pinMode(KEY1, OUTPUT);
  pinMode(KEY2, OUTPUT);
  pinMode(KEY3, OUTPUT);
  pinMode(KEY4, OUTPUT);
  pinMode(KEY5, OUTPUT);
  pinMode(PIEZO, OUTPUT);
  pinMode(GEN, OUTPUT);
  pinMode(DOT, OUTPUT);
  pinMode(BACKLR, OUTPUT);
  pinMode(BACKLG, OUTPUT);
  pinMode(BACKLB, OUTPUT);
  
  digitalWrite(GEN, 0);                           // устранение возможного "залипания" выхода генератора
  btnSet.setTimeout(400);                         // установка параметров библиотеки реагирования на кнопки
  btnSet.setDebounce(90);                         // защитный период от дребезга
  btnR.setDebounce(90);                           // защитный период от дребезга
  btnL.setDebounce(90);                           // защитный период от дребезга

 // ---------- RTC -----------
  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  pinMode(RTC_SYNC, INPUT_PULLUP);                // объявляем вход для синхросигнала RTC
                                                  // заставляем входной сигнал генерировать прерывания
  attachInterrupt(digitalPinToInterrupt(RTC_SYNC), RTC_handler, RISING);
  rtc.writeSqwPinMode(DS3231_SquareWave8kHz);     // настраиваем DS3231 для вывода сигнала 8кГц

  // настройка быстрого чтения аналогового порта (mode 4)
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
  analogRead(A6);                                 // устранение шума
  analogRead(A7);
  // ------------------
  boolean time_sync = false;
  DateTime now = rtc.now();
  do {
    if (!time_sync) {
      time_sync = true;
      secs = now.second();
      mins = now.minute();
      hrs = now.hour();
    }
    now = rtc.now();
  } while (secs != now.second());
  secs = now.second();
  SQW_counter = 0;
  mins = now.minute();
  hrs = now.hour();
      
  // задаем частоту ШИМ на 9 и 10 выводах 31 кГц
  TCCR1B = TCCR1B & 0b11111000 | 1;               // ставим делитель 1

  // перенастраиваем частоту ШИМ на пинах 3 и 11 для соответствия таймеру 0
  // Пины D3 и D11 - 980 Гц
  TCCR2B = 0b00000011;                            // x32
  TCCR2A = 0b00000001;                            // phase correct

  // EEPROM
  if (EEPROM.read(1023) != 103) {                 // первый запуск
    EEPROM.put(1023, 103);
    EEPROM.put(FLIPEFF, FLIP_EFFECT);
    EEPROM.put(LIGHTEFF, BACKL_MODE);
    EEPROM.put(GLEFF, GLITCH_ALLOWED);
    EEPROM.put(ALHOUR, 0);
    EEPROM.put(ALMIN, 0);
    EEPROM.put(ALIFSET, false);
    EEPROM.put(BLCOLOR, 1);
  }
  EEPROM.get(FLIPEFF, FLIP_EFFECT);
  EEPROM.get(LIGHTEFF, BACKL_MODE);
  EEPROM.get(GLEFF, GLITCH_ALLOWED);
  EEPROM.get(ALHOUR, alm_hrs);
  EEPROM.get(ALMIN, alm_mins);
  EEPROM.get(ALIFSET, alm_set);
  EEPROM.get(BLCOLOR, backlColor);

  // включаем ШИМ
  r_duty = DUTY;
  setPWM(GEN, r_duty);
 
  sendTime(hrs, mins, secs);                      // отправить время на индикаторы
  changeBright();                                 // изменить яркость согласно времени суток

  // стартовый период глюков
  glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));

  // скорость режима при запуске
  flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);

  // инициализация BME
  isBMEhere = bme.begin();
  if( !isBMEhere) {
    isBMEhere = bme.begin(BME280_ADDRESS_ALTERNATE);
  }
  if (isBMEhere ) bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
}
