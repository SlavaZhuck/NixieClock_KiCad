
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

/* Замкнутая система регулирования анодного (высокого) напряжения.
 *
 * Период вызова DCDCTick — основной loop(); измерение выполняется на A6.
 * Регулятор интегральный: ошибка между измеренным напряжением и номиналом
 * накапливается за iduty вызовов; при превышении порога maxerrduty
 * скважность ШИМ генератора (r_duty) корректируется на один шаг и ограничивается
 * диапазоном [minduty, maxduty]. Любое отклонение от этой логики напрямую
 * влияет на стабильность HV — будьте осторожны при правках.
 */

static const uint8_t iduty       = 10;    // период интегрирования ошибки (кол-во вызовов)
static const int8_t  maxerrduty  = 10;    // порог срабатывания
static const int     nominallevel = 490;  // целевое значение АЦП
static const uint8_t maxduty     = 220;   // ограничение скважности сверху
static const uint8_t minduty     = 10;    // ограничение снизу

static uint8_t idcounter = 0;             // текущая позиция в периоде интегрирования
static int     duty_delta = 0;            // накопленная ошибка

uint8_t r_duty;                           // актуальная скважность ШИМ
int8_t  startup_delay = 10;               // задержка после старта, кратно 500мс

void DCDCTick(void)
{
  int voltage = analogRead(A6);
  if (startup_delay > 0) return;          // ждём стабилизации после запуска

  // интегрируем ошибку; по завершении периода сбрасываем счётчик
  if (++idcounter == iduty)
  {
    duty_delta = voltage - nominallevel;
    idcounter = 0;
  }
  else
  {
    duty_delta += voltage - nominallevel;
  }

  if (duty_delta > maxerrduty)
  { // напряжение выше номинала — уменьшаем скважность
    duty_delta = 0;
    if (r_duty > minduty) setPWM(GEN, --r_duty);
    else                  setPWM(GEN, 0);
  }
  else if (duty_delta < -maxerrduty)
  { // напряжение ниже номинала — увеличиваем скважность
    duty_delta = 0;
    if (++r_duty > maxduty) r_duty = maxduty;
    else                    setPWM(GEN, r_duty);
  }
}
