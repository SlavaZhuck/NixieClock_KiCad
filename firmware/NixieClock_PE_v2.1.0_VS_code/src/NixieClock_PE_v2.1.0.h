/*----------- Версия PE 2.1.0 ---------------*/

/* Исходная версия: ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓
  Скетч к проекту "Часы на ГРИ версия 2"
  Страница проекта (схемы, описания): https://alexgyver.ru/nixieclock_v2/
  Исходники на GitHub: https://github.com/AlexGyver/NixieClock_v2
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  https://AlexGyver.ru/
  ------------------------------------------------------------------------------
  Изменено для другой разводки платы: Потапов Владислав:
  - универсальная версия на 4/6 ламп;
  - добавлены секунды (в версию на 6 ламп);
  - тактирование переведено на SQW-выход DS3231 (с прерыванием), прерывание таймера оригинального скетча (таймер 2) не используется;
  - таймер 0 и таймер 2 используются на одинаковых частотах = таймер 0, для управления тремя линиями светодиодов (PWM);
  - таймер 1 используется на частоте 32кГц для PWM генератора DC/DC и яркости неонки;
  - контроль выходного высокого напряжения через делитель на вывод А6;
  - подключен будильник, добавлено меню установки будильника и режим просмотра установки будильника;
  - мелодия будильника реализована на программной реализации мелодии в прерывании SQW;
  - индикация включения будильника реализована через разные режимы моргания секундной точки;
  - часть эффектов переключения цифр срабатывает только при смене минут;
  - исправлен показ номера эффекта при переключении;
  - подключение BME-280, три режима для отображения результатов измерений.
*/

/*
  Управление:
  - При отображении часов:
    - М       (двойной клик) - войти в режим настроек времени;
    -         (удержание) - войти в режим настроек будильника;
    - "минус" (кратко) - переключает режимы подсветки ламп;
    -         (удержание) - включает/отключает "глюки";
    - "плюс"  (кратко) - переключает режимы перелистывания цифр;
    -         (удержание) - автопоказывать измерения температуры, давления и влажности
    - сенсор  (кратко) - показать температуру, давление, влажность, установленное время будильника
              (только если будильник включен) и вернуться в режим отображения часов;
    -         (удержание) - то же, что (кратко).
    
  - При срабатывании будильника (играет мелодия):
    - сенсор  (кратко) - сброс сигнала, будильник остаётся включенным;
    - сенсор  (удержание) - сброс сигнала, будильник остаётся включенным.
    
  - При демонстрации температуры, влажности, давлении, времени будильника:
    - сенсор  (кратко) - переключиться на следующий параметр (давление, влажность, установленное
              время будильника, отображение часов);
    -         (удержание) - вернуться в режим отображения часов;
    
  - При настройке времени:
    - М       (кратко) - переключение между установкой часов и минут;
    -         (удержание) - сброс текущей группы разрядов в 00;
    - "минус" (кратко) - уменьшение значения;
    -         (удержание) - уменьшение значения на 5;
    - "плюс"  (кратко) - увеличение значения;
    -         (удержание) - уведичение значения на 5;
    - сенсор  (кратко) - выход с сохранением установок;
    - сенсор  (удержание) - выход с возвратом к прежнему значению.
    
  - При настройке будильника:
    - М       (кратко) - переключение между установкой часов и минут;
    -         (удержание) - сброс текущей группы разрядов в 00;
    - "минус" (кратко) - уменьшение значения;
    -         (удержание) - уменьшение значения на 5;
    - "плюс"  (кратко) - увеличение значения;
    -         (удержание) - уведичение значения на 5;
    - сенсор  (кратко) - выход с сохранением установок;
    - сенсор  (удержание) - включение/выключение будильника.
    
  - Эффекты В РЕЖИМЕ ЧАСОВ:
    - Подсветка (циклически изменяется цвет: красный,  зелёный, синий):
      - Дыхание;
      - Постоянное свечение;
      - Отключена.
    - Смена цифр (при смене на короткое время отображается номер эффекта во всех разрядах):
      - (0) Без эффекта;
      - (1) Плавное угасание;
      - (2) Перемотка по порядку числа;
      - (3) Перемотка по катодам;
      - (4) Поезд;
      - (5) Резинка.
    - Поведение секундной точки зависит от того, включен ли будильник? устанавливается параметрами:
      - DOT_IN_TIME - когда будильник выключен;
      - DOT_IN_ALARM - когда будильник включен.
      Выбор поведения точки можно осуществлять из следующих величин:
      - DM_NULL, (0) точка постоянно выключена;
      - DM_ONCE, (1) точка моргает один раз в секунду (штатно);
      - DM_HALF, (2) точка изменяет яркость раз в секунду;
      - DM_TWICE,(3) точка моргает два раза в секунду;
      - DM_THREE,(4) точка моргает три раза в секунду;
      - DM_FULL, (5) точка постоянно включена
*/
#include <Arduino.h>
// ************************** НАСТРОЙКИ **************************
#define BOARD_TYPE 3 //ONLY 3 is supported
// тип платы часов:
// 0 - IN-12 turned (индикаторы стоят правильно)
// 1 - IN-12 (индикаторы перевёрнуты)
// 2 - IN-14 (обычная и neon dot)
// 3 - COVID 2019 (проект ADM503 и poty)
// 4 - "Ладушки" ИН-12
// 5 - "Ладушки" ИН-14

#define DUTY 180                                            // начальная скважность ШИМ

// ======================= ЭФФЕКТЫ =======================
                                                            // эффекты перелистывания часов
enum FLIP_MODES: byte
                   { FM_NULL,                               // (0) нет эффекта
                     FM_SMOOTH,                             // (1) плавное угасание и появление (рекомендуемая скорость: 100-150), срабатывает только на минутах
                     FM_LIST,                               // (2) перемотка по порядку числа (рекомендуемая скорость: 50-80)
                     FM_CATHODE,                            // (3) перемотка по порядку катодов в лампе (рекомендуемая скорость: 30-50)
                     FM_TRAIN,                              // (4) поезд (рекомендуемая скорость: 50-170), срабатывает только на минутах
                     FM_ELASTIC };                          // (5) резинка (рекомендуемая скорость: 50-150), срабатывает только на минутах
byte FLIP_EFFECT = FM_SMOOTH;                               // Выбранный активен при первом запуске и меняется кнопками.
                                                            // Запоминается в память

// =======================  ЯРКОСТЬ =======================
#define NIGHT_LIGHT       1                                 // менять яркость от времени суток (1 вкл, 0 выкл)
#define NIGHT_START       23                                // час перехода на ночную подсветку (BRIGHT_N)
#define NIGHT_END         7                                 // час перехода на дневную подсветку (BRIGHT)

#define INDI_BRIGHT       23                                // яркость цифр дневная (1 - 24) !на 24 могут быть фантомные цифры!
#define INDI_BRIGHT_N     3                                 // яркость ночная (1 - 24)

#define DOT_BRIGHT        30                                // яркость точки дневная (1 - 255)
#define DOT_BRIGHT_N      5                                 // яркость точки ночная (1 - 255)

#define BACKL_BRIGHT      80                               // макс. яркость подсветки ламп дневная (0 - 255)
#define BACKL_BRIGHT_N    40                                // макс. яркость подсветки ламп ночная (0 - 255, 0 - подсветка выключена)
#define BACKL_MIN_BRIGHT  20                                // мин. яркость подсветки ламп в режиме дыхание (0 - 255)
#define BACKL_PAUSE       400                               // пауза "темноты" между вспышками подсветки ламп в режиме дыхание, мс

// =======================  ГЛЮКИ =======================
#define GLITCH_MIN        30                                // минимальное время между глюками, с
#define GLITCH_MAX        120                               // максимальное время между глюками, с

// ======================  МИГАНИЕ =======================
#define DOT_TIME          100                               // время мигания точки, мс
#define DOT_TIMER         20                                // шаг яркости точки, мс

enum DOT_MODES: byte
                   { DM_NULL,                               // (0) точка постоянно выключена
                     DM_ONCE,                               // (1) точка моргает один раз в секунду (штатно)
                     DM_HALF,                               // (2) точка изменяет яркость раз в секунду
                     DM_TWICE,                              // (3) точка моргает два раза в секунду
                     DM_THREE,                              // (4) точка моргает три раза в секунду
                     DM_FULL };                             // (5) точка постоянно включена
#define DOT_IN_TIME       DM_ONCE
#define DOT_IN_ALARM      DM_TWICE

#define BACKL_STEP        2                                 // шаг мигания подсветки
#define BACKL_TIME        5000                              // период подсветки, мс

// ==================  АНТИОТРАВЛЕНИЕ ====================
#define BURN_TIME         10                                // период обхода индикаторов в режиме очистки, мс
#define BURN_LOOPS        3                                 // количество циклов очистки за каждый период
#define BURN_PERIOD       15                                // период антиотравления, минут


// *********************** ДЛЯ РАЗРАБОТЧИКОВ ***********************
byte BACKL_MODE = 0;                                        // Выбранный режим активен при запуске и меняется кнопками
                                                            // скорость эффектов, мс (количество не меняй)
byte FLIP_SPEED[] = {0, 60, 50, 40, 90, 90}; 
                                                            // количество эффектов
byte FLIP_EFFECT_NUM = sizeof(FLIP_SPEED);    
boolean GLITCH_ALLOWED = 1;                                 // 1 - включить, 0 - выключить глюки. Управляется кнопкой
boolean auto_show_measurements = 1;                         // автоматически показывать измерения Temp, Bar, Hum
// --------- БУДИЛЬНИК ---------
#define ALM_TIMEOUT       30                                // таймаут будильника

// ---------   ПИНЫ    ---------
#define RTC_SYNC          2                                 // - подключение SQW выхода
#define PIEZO             1                                 // - подключение пищалки
#define AV_CTRL           A6                                // - вход контроля анодного напряжения

#define KEY0              8                                 // - часы (десятки)
#define KEY1              3                                 // - часы (единицы)
#define KEY2              4                                 // - минуты (десятки)
#define KEY3              13                                // - минуты (единицы)
#define KEY4              0                                 // - секунды (десятки) - исправить в v.1 подключение!!!! было подключено к 6
#define KEY5              7                                 // - секунды (единицы)

#define GEN               9                                 // - генератор
#define DOT               10                                // - точка
#define BACKLR            11                                // - выход на красные светодиоды подсветки
#define BACKLG            6                                 // - выход на зелёные светодиоды подсветки
#define BACKLB            5                                 // - выход на голубые светодиоды подсветки
#define ALARM_STOP        12                                // - кнопка остановки сигнала будильника
// дешифратор
#define DECODER0          A0                                // -
#define DECODER1          A1                                // -
#define DECODER2          A2                                // -
#define DECODER3          A3                                // -
// A4, A5 - I2C
// A7 - аналоговая клавиатура

#define NUMTUB            6                                 // количество разрядов (ламп)

#define NUMMENU           4                                 // количество разрядов в меню

// распиновка ламп
const byte digitMask[] = {8, 9, 0, 1, 5, 2, 4, 6, 7, 3};    // маска дешифратора платы COVID-19 (подходит для ИН-14 и ИН-12)
const byte opts[] = {KEY0, KEY1, KEY2, KEY3, KEY4, KEY5};   // порядок индикаторов слева направо
const byte cathodeMask[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};  // порядок катодов

/*
  ард ног ном
  А0  7   4
  А1  6   2
  А2  4   8
  А3  3   1
*/

void dotSetMode(DOT_MODES dMode);
