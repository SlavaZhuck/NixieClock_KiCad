#ifndef PERIPHERALS_EXT_H
#define PERIPHERALS_EXT_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <GyverButton.h>

// часы реального времени
extern RTC_DS3231 rtc;

// датчик BME280
extern Adafruit_BME280 bme;
extern Adafruit_Sensor *bme_temp;
extern Adafruit_Sensor *bme_pressure;
extern Adafruit_Sensor *bme_humidity;
extern boolean isBMEhere;

// кнопки
extern GButton btnSet;
extern GButton btnL;
extern GButton btnR;
extern GButton btnA;

// генератор анодного напряжения
extern uint8_t r_duty;

#endif
