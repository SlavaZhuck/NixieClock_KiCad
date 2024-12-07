
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"


/* регулировка напряжения */
const uint8_t iduty = 10;                         // период интегрирования ошибки напряжения
uint8_t idcounter = 0;                            // текущий период интегрирования
const int8_t maxerrduty = 10;                     // максимальное значение интегрированной ошибки напряжения
int duty_delta = 0;                               // текущая интегральная ошибка
const int nominallevel = 490;                     // номинальное значение напряжения
const uint8_t maxduty = 220;                      // защита от чрезмерного повышения напряжения
const uint8_t minduty = 10;                       // защита от выключения
uint8_t r_duty;                                   // актуальная скважность ШИМ анодного напряжения
int8_t startup_delay = 5;                         // задержка в применении нового значения ШИМ после запуска

/* Стабилизация высокого напряжения
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void DCDCTick(void) {
  int voltage;
  
  voltage = analogRead(A6);                                               // читаем значение анодного напряжения
  if(startup_delay <=0) {                                                 // ждём первоначальной установки напряжения
    if(++idcounter == iduty) {                                            // пройден полный цикл усреднения
      duty_delta = voltage - nominallevel;                                // текущая разница с идеальным напряжением - первая
      idcounter = 0;                                                      // обновляем цикл усреднения
    } else duty_delta += voltage - nominallevel;                          // интегрируем ошибку
    if ( duty_delta > 0 && duty_delta > maxerrduty ) {                    // измеренное напряжение выше идеального и превышает допустимую ошибку
      duty_delta = 0;                                                     // обнуляем интегральную ошибку
      if (r_duty > minduty) {                                             // если есть ещё возможность уменьшить длину активного импульса
        setPWM(GEN, --r_duty);                                            // уменьшаем длину активного импульса и устанавливаем новые значения ШИМ
      } else setPWM(GEN, 0);                                              // иначе выключаем генерацию
    } else if ( duty_delta < 0 && duty_delta < -maxerrduty ) {            // измеренное значение ниже идеального и превышает допустимую ошибку
      duty_delta = 0;                                                     // обнуляем интегральную ошибку
      if (++r_duty > maxduty ) r_duty = maxduty;                          // если нет возможности увеличить длину активного импульса - оставляем максимальную
      else {
        setPWM(GEN, r_duty);                                              // если есть возможности увеличить длину активного импульса - устанавливаем новый ШИМ
      }
    }
  }
}