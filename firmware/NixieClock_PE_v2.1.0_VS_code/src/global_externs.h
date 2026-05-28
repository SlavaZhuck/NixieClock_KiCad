#ifndef GLOBAL_EXTERNS_H
#define GLOBAL_EXTERNS_H

// Тонкий фасад, агрегирующий доменные заголовки.
// Для новых файлов предпочтительнее включать только необходимый домен:
//   display_ext.h     — индикация, низкоуровневый ввод/вывод пинов и ШИМ
//   clock_ext.h       — время, синхронизация с RTC, коррекция дрейфа
//   peripherals_ext.h — RTC, BME280, кнопки, ШИМ-генератор
//   ui_ext.h          — режимы интерфейса, будильник, таймеры, эффекты

#include "display_ext.h"
#include "clock_ext.h"
#include "peripherals_ext.h"
#include "ui_ext.h"

#endif
