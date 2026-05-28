#ifndef UI_EXT_H
#define UI_EXT_H

#include <Arduino.h>
#include "timer2Minim.h"
#include "NixieClock_PE_v2.1.0.h"

// текущий режим интерфейса
extern SH_MODES curMode;

// будильник
extern int8_t alm_hrs, alm_mins;
extern boolean alm_set;
extern boolean alm_flag;

// таймеры режимов
extern timerMinim flipTimer;
extern timerMinim eshowTimer;
extern timerMinim autoShowMeasurementsTimer;
extern timerMinim measurementsTimer;
extern timerMinim autoTimer;
extern timerMinim glitchTimer;

// эффекты для цифр
extern byte flip_effect;
extern byte flip_speed[];
extern byte flip_effect_num;

// подсветка ламп и глюки
extern byte backL_mode;
extern byte backlColors[];
extern byte backlColor;
extern boolean chBL;
extern boolean glitch_allowed;
extern boolean auto_show_measurements;

#endif
