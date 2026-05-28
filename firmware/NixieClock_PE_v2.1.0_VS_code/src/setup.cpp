
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

#include <EEPROM.h>

// перечисление пинов на вывод — настраиваются как OUTPUT одним циклом.
// Размещено в PROGMEM, чтобы не занимать RAM.
static const byte outputPins[] PROGMEM = {
  DECODER0, DECODER1, DECODER2, DECODER3,
  KEY0, KEY1, KEY2, KEY3, KEY4, KEY5,
  PIEZO, GEN, DOT,
  BACKLR, BACKLG, BACKLB,
};

/* Конфигурация GPIO. */
static void setupPins(void)
{
  pinMode(ALARM_STOP, INPUT);
  for (byte i = 0; i < sizeof(outputPins); i++)
    pinMode(pgm_read_byte(&outputPins[i]), OUTPUT);
  digitalWrite(GEN, 0); // устранение возможного «залипания» выхода генератора
}

/* Параметры обработки кнопок (общий debounce и таймаут на кнопке Set). */
static void setupButtons(void)
{
  btnSet.setTimeout(400);
  btnSet.setDebounce(90);
  btnL.setDebounce(90);
  btnR.setDebounce(90);
}

/* Инициализация DS3231 и подключение прерывания SQW. */
static void setupRtc(void)
{
  rtc.begin();
  if (rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  pinMode(RTC_SYNC, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_SYNC), RTC_handler, RISING);
  rtc.writeSqwPinMode(DS3231_SquareWave8kHz);
}

/* Перенастройка ATmega328 АЦП и таймеров под нужды проекта. */
static void setupAvrHardware(void)
{
  // быстрое чтение АЦП (mode 4)
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
  analogRead(A6); // первое чтение — устранение шума
  analogRead(A7);

  // ШИМ 31 кГц на D9/D10 (Timer1)
  TCCR1B = (TCCR1B & 0b11111000) | 1;

  // ШИМ ~980 Гц на D3/D11 (Timer2)
  TCCR2B = 0b00000011; // делитель x32
  TCCR2A = 0b00000001; // phase correct
}

/* Загрузка/инициализация настроек из EEPROM. */
static void setupEeprom(void)
{
  if (EEPROM.read(1023) != 103)
  { // первый запуск — заполнить значениями по умолчанию
    EEPROM.put(1023, 103);
    EEPROM.put(FLIPEFF, flip_effect);
    EEPROM.put(LIGHTEFF, backL_mode);
    EEPROM.put(GLEFF, glitch_allowed);
    EEPROM.put(ALHOUR, 0);
    EEPROM.put(ALMIN, 0);
    EEPROM.put(ALIFSET, false);
    EEPROM.put(BLCOLOR, 1);
    EEPROM.put(AUTOSHOWMEAS, 1);
    EEPROM.put(LASTADJMONTH, (byte)0);
    EEPROM.put(AUTOADJVAL, (int8_t)0);
  }
  EEPROM.get(FLIPEFF, flip_effect);
  EEPROM.get(LIGHTEFF, backL_mode);
  EEPROM.get(GLEFF, glitch_allowed);
  EEPROM.get(ALHOUR, alm_hrs);
  EEPROM.get(ALMIN, alm_mins);
  EEPROM.get(ALIFSET, alm_set);
  EEPROM.get(BLCOLOR, backlColor);
  EEPROM.get(AUTOSHOWMEAS, auto_show_measurements);
  EEPROM.get(LASTADJMONTH, lastAdjustedMonth);
  EEPROM.get(AUTOADJVAL, autoAdjustTimeValue);

  // повреждённое/неинициализированное значение — синхронизируемся с RTC
  // (без применения сдвига; коррекция сработает при следующей смене месяца)
  if (lastAdjustedMonth < 1 || lastAdjustedMonth > 12)
  {
    lastAdjustedMonth = rtc.now().month();
    EEPROM.put(LASTADJMONTH, lastAdjustedMonth);
  }
  // если значение поправки повреждено — сбросить в 0
  if (autoAdjustTimeValue < -99 || autoAdjustTimeValue > 99)
  {
    autoAdjustTimeValue = 0;
    EEPROM.put(AUTOADJVAL, autoAdjustTimeValue);
  }
  // AUTOADJVAL добавлено в прошивке позже; при первом запуске новой прошивки
  // EEPROM по адресу AUTOADJVAL содержит 0xFF (-1) — инициализируем в 0
  if (EEPROM.read(1022) != 1)
  {
    EEPROM.put(1022, (byte)1);
    autoAdjustTimeValue = 0;
    EEPROM.put(AUTOADJVAL, autoAdjustTimeValue);
  }
}

/* Поиск и настройка BME280 (две возможные I2C-адресации). */
static void setupBme(void)
{
  isBMEhere = bme.begin();
  if (!isBMEhere)
    isBMEhere = bme.begin(BME280_ADDRESS_ALTERNATE);
  if (isBMEhere)
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X16, // temperature
                    Adafruit_BME280::SAMPLING_X16, // pressure
                    Adafruit_BME280::SAMPLING_X16, // humidity
                    Adafruit_BME280::FILTER_X4);
}

/* Стартовая инициализация системы. */
void setup()
{
  randomSeed(analogRead(6) + analogRead(7));

  setupPins();
  setupButtons();
  setupRtc();
  setupAvrHardware();
  syncFromRtc();
  setupEeprom();

  // запуск ШИМ генератора анодного напряжения
  r_duty = DUTY;
  setPWM(GEN, r_duty);

  sendTime(hrs, mins, secs, indiDigits);
  changeBright();

  // стартовый период между «глюками» и скорость текущего эффекта цифр
  glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));
  flipTimer.setInterval(flip_speed[flip_effect]);

  setupBme();
}
