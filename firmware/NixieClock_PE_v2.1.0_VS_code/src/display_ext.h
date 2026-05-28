#ifndef DISPLAY_EXT_H
#define DISPLAY_EXT_H

#include <Arduino.h>
#include "timer2Minim.h"

// функции отображения данных на индикаторах
extern void sendTime(byte hours, byte minutes, byte seconds, volatile int8_t indiDigitsLocal[]);
extern void sendYear(uint16_t year, volatile int8_t indiDigitsLocal[]);
extern void sendDate(byte month, byte day, volatile int8_t indiDigitsLocal[]);
extern void setNewTime(byte hours, byte minutes, byte seconds, byte newTimeLocal[]);
extern void burnIndicators(void);

// низкоуровневая работа с пинами/ШИМ
extern void setPWM(byte pin, byte duty);
extern void setPin(byte pin, byte x);
extern byte getPWM_CRT(byte val);

// состояние индикаторов
extern volatile int8_t indiDigits[]; // цифры, которые должны показать индикаторы (0-10)
extern volatile int8_t indiDimm[];   // величина диммирования (0-24)
extern byte newTime[];
extern byte anodeStates;
extern byte indiMaxBright;
extern int  indiBrightCounter;
extern boolean newTimeFlag;
extern boolean newSecFlag;

// точка
extern boolean dotBrightFlag, dotBrightDirection;
extern int dotBrightCounter;

#endif
