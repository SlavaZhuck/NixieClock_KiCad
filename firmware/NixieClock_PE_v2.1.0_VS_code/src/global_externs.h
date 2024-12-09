#ifndef GLOBAL_EXTERNS_H
#define GLOBAL_EXTERNS_H

#include <GyverButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

extern void sendTime(byte hours, byte minutes, byte seconds);
extern void changeBright(void);
extern void burnIndicators(void);
extern void setNewTime(void);
extern void RTC_handler(void);
extern void setPWM(byte pin, byte duty);
extern void setPin(byte pin, byte x);
extern byte getPWM_CRT(byte val);

extern int8_t hrs, mins, secs;
extern boolean newTimeFlag;
extern boolean newSecFlag;
extern volatile int8_t indiDigits[];

extern int indiBrightCounter;
extern byte indiMaxBright;
extern byte newTime[];

extern volatile int8_t indiDimm[];    // величина диммирования (0-24)

extern volatile int8_t indiDigits[];  // цифры, которые должны показать индикаторы (0-10)

extern byte anodeStates;
extern timerMinim flipTimer;
extern byte FLIP_EFFECT;
extern byte FLIP_SPEED[];
extern boolean alm_flag;
extern byte FLIP_EFFECT_NUM;
extern boolean auto_show_measurements;
extern SH_MODES curMode;
extern timerMinim eshowTimer;
extern timerMinim autoShowMeasurementsTimer;
extern timerMinim measurementsTimer;
extern timerMinim autoTimer;
extern timerMinim glitchTimer;
extern boolean showFlag;
extern byte BACKL_MODE;
extern boolean GLITCH_ALLOWED;
extern byte backlColors[];
extern byte backlColor;
extern boolean chBL;
extern boolean isBMEhere;

extern GButton btnSet;
extern GButton btnL;
extern GButton btnR;
extern GButton btnA;

extern RTC_DS3231 rtc;
extern Adafruit_BME280 bme;
extern Adafruit_Sensor *bme_temp;
extern Adafruit_Sensor *bme_pressure;
extern Adafruit_Sensor *bme_humidity;
extern boolean currentDigit;
extern int8_t alm_hrs, alm_mins;
extern boolean alm_set;
extern volatile unsigned int SQW_counter;

extern volatile boolean halfsecond;
extern boolean dotBrightFlag, dotBrightDirection;
extern int dotBrightCounter;
extern int8_t startup_delay;

extern uint8_t r_duty;

extern const uint8_t NotePrescalerHigh[];
extern const uint8_t notecounter;
extern volatile unsigned int note_num;   // номер ноты в мелодии
extern volatile unsigned int note_count; // фаза сигнала
extern volatile boolean note_up_low;     // true - высокий уровень, false - низкий уровень
extern volatile boolean note_ip;         // true - сигнал звучит, false - сигнал не звучит
#endif