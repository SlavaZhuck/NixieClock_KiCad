# Clock on gas-discharge Nixie indicators  "IN-14" and Arduino
A fork of the adm503 project for those who want to be able to make changes to the circuit and/or board layout.
The circuit is copied with minimal changes (resistors, diode and transistor are changed to smd). The board and circuit are implemented in KiCad for 6 IN-14 lamps.<br>
The board has not been assembled and tested. It may not be. Please consider this information when using.<br>
The firmware has been updated to work with the DS3231M clock module.
The ability to automatically display data from the pressure, humidity and temperature sensor has been added.

Controls:
- When displaying the clock:
- M (double click) - enter the time settings mode;
- (hold) - enter the alarm settings mode;
- "minus" (short) - switches the lamp backlight modes;
- (hold) - enables/disables "glitches";
- "plus" (short) - switches the digit scrolling modes;
- (hold) - auto show temperature, pressure and humidity measurements
- sensor (short) - show temperature, pressure, humidity, set alarm time
(only if the alarm is on) and return to the clock display mode;
- (hold) - the same as (short).

- When the alarm goes off (a melody plays):
- sensor (short) - reset the signal, the alarm remains on;
- sensor (hold) - reset the signal, the alarm remains on.

- When displaying temperature, humidity, pressure, alarm time:
- sensor (short) - switch to the next parameter (pressure, humidity, set alarm time, clock display);
- (hold) - return to the clock display mode;

- When setting the time:
- M (short) - switch between setting hours and minutes;
- (hold) - reset the current group of digits to 00;
- "minus" (short) - decrease the value;
- (hold) - decrease the value by 5;
- "plus" (short) - increase the value;
- (hold) - increase the value by 5;
- sensor (short) - exit saving the settings;
- sensor (hold) - exit returning to the previous value.

- When setting the alarm:
- M (short) - switch between setting hours and minutes;
- (hold) - reset the current group of digits to 00;
- "minus" (short) - decrease the value;
- (hold) - decrease the value by 5;
- "plus" (short) - increase the value;
- (hold) - increase the value by 5;
- sensor (short) - exit with saving settings;
- sensor (hold) - turn on/off the alarm.

- Effects IN CLOCK MODE:
- Backlight (color changes cyclically: red, green, blue):
- Breathing;
- Constant glow;
- Disabled.
- Change digits (during change, the effect number is briefly displayed in all digits):
- (0) No effect;
- (1) Smooth fading;
- (2) Rewind in numerical order;
- (3) Rewind by cathodes;
- (4) Train;
- (5) Rubber band.
- The behavior of the second dot depends on whether the alarm is on? is set by the parameters:
- DOT_IN_TIME - when the alarm is off;
- DOT_IN_ALARM - when the alarm is on.
The dot behavior can be selected from the following values:
- DM_NULL, (0) the dot is always off;
- DM_ONCE, (1) the dot blinks once a second (normal);
- DM_HALF, (2) the dot changes brightness once a second;
- DM_TWICE, (3) the dot blinks twice a second;
- DM_THREE, (4) the dot blinks three times a second;
- DM_FULL, (5) the dot is always on


# Часы на газоразрядных индикаторах "ИН-14" и Arduino
Форк проекта adm503 для желающих иметь возможность сделать изменения в схеме и\или разводке платы.
Схема скопирована с минимальными изменениями (резисторы, диод и транзистор изменены на smd). Плата и схема реализованы в KiCad для 6-ти ламп ИН-14.<br>
Плата не была собрана и проверена. Возможно и не будет. Учитывайте пожалуйста эту информацию при использовании. <br>

Была обновлена прошивка для работы с модулем часов DS3231M.
Добавлена возможность автоматического показания данных с сенсора давления, влажности и температуры.
Управление:
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
