

extern void sendTime(byte hours, byte minutes, byte seconds, volatile int8_t indiDigitsLocal[]);
extern void setNewTime(byte hours, byte minutes, byte seconds, byte newTimeLocal[]);
extern void sendYear(uint16_t year, volatile int8_t indiDigitsLocal[]);
extern void sendDate(byte month, byte day, volatile int8_t indiDigitsLocal[]);
extern void sendAutoAdjust(int8_t val, volatile int8_t indiDigitsLocal[]);
