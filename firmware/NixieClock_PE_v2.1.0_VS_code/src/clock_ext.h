#ifndef CLOCK_EXT_H
#define CLOCK_EXT_H

#include <Arduino.h>
#include <RTClib.h>

// текущее время (часы/минуты/секунды) и фаза полусекунды
extern int8_t hrs, mins, secs;
extern volatile boolean halfsecond;
extern volatile unsigned int SQW_counter;
extern int8_t startup_delay;

// автоподстройка по календарному месяцу
extern byte lastAdjustedMonth;
extern int8_t autoAdjustTimeValue;  // поправка кварца, с/мес (−99…+99)
extern boolean monthAdjustPending;  // флаг: поправка ждёт 30-й секунды

// функции синхронизации/обработки времени
extern void RTC_handler(void);
extern DateTime syncFromRtc(void);
extern void changeBright(void);

#endif
