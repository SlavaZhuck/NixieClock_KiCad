
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include <GyverButton.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "global_externs.h"


byte glitchCounter, glitchMax, glitchIndic;
boolean glitchFlag, indiState;

/* check 28.10.20 */

/* Обеспечение работы "глюков"
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */
void glitchTick(void)
{
  if (!glitchFlag && secs > 7 && secs < 55)
  {
    if (glitchTimer.isReady())
    {
      glitchFlag = true;
      indiState = 0;
      glitchCounter = 0;
      glitchMax = random(2, 6);
      glitchIndic = random(0, NUMTUB);
      glitchTimer.setInterval(random(1, 6) * 20);
    }
  }
  else if (glitchFlag && glitchTimer.isReady())
  {
    indiDimm[glitchIndic] = indiState * indiMaxBright;
    indiState = !indiState;
    glitchTimer.setInterval(random(1, 6) * 20);
    glitchCounter++;
    if (glitchCounter > glitchMax)
    {
      glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));
      glitchFlag = false;
      indiDimm[glitchIndic] = indiMaxBright;
    }
  }
}