
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

// Эффекты перелистывания цифр индикаторов. Каждый эффект реализован в своей
// функции, диспетчер flipTick() выбирает её по текущему flip_effect.

static boolean flipInit;
static boolean indiBrightDirection;
static boolean flipIndics[NUMTUB];
static byte startCathode[NUMTUB], endCathode[NUMTUB];
static byte currentLamp, flipEffectStages;
static bool trainLeaving;
static const byte cathodeMask[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};

/* Помечаем разряды, в которых цифра изменилась — для них и проигрывается эффект. */
static void markChangedDigits(void)
{
  for (byte i = 0; i < NUMTUB; i++)
    flipIndics[i] = (indiDigits[i] != newTime[i]);
}

/* Обновить только секунды в разрядах 4..5 (используется, когда сменились секунды
 * на фоне идущего минутного эффекта).
 */
static void refreshSecondsDigits(void)
{
  indiDigits[4] = (byte)secs / 10;
  indiDigits[5] = (byte)secs % 10;
  newSecFlag = false;
}

/* FM_NULL: эффект отсутствует, время отображается мгновенно. */
static void flipNull(void)
{
  sendTime(hrs, mins, secs, indiDigits);
  newTimeFlag = false;
  newSecFlag = false;
}

/* FM_SMOOTH: плавное угасание разрядов, в которых цифра меняется,
 * затем плавное появление новой цифры.
 */
static void flipSmooth(void)
{
  if (!newTimeFlag)
  {
    if (newSecFlag)
      refreshSecondsDigits();
    return;
  }

  if (newSecFlag) // секунды также сменились — обновить мгновенно
    refreshSecondsDigits();

  if (!flipInit)
  {
    flipInit = true;
    flipTimer.setInterval((unsigned long)flip_speed[flip_effect]);
    flipTimer.reset();
    indiBrightDirection = false;
    markChangedDigits();
  }

  if (!flipTimer.isReady()) return;

  if (!indiBrightDirection)
  { // уменьшаем яркость до нуля
    if (--indiBrightCounter <= 0)
    {
      indiBrightDirection = true;
      indiBrightCounter = 0;
      sendTime(hrs, mins, secs, indiDigits); // подменили цифры в точке нулевой яркости
    }
  }
  else
  { // увеличиваем обратно до максимума
    if (++indiBrightCounter >= indiMaxBright)
    {
      indiBrightDirection = false;
      indiBrightCounter = indiMaxBright;
      flipInit = false;
      newTimeFlag = false;
    }
  }
  for (byte i = 0; i < NUMTUB; i++)
    if (flipIndics[i])
      indiDimm[i] = indiBrightCounter;
}

/* Общий «cleanup» для эффектов LIST/CATHODE — все цифры пришли к нужному значению. */
static boolean allDigitsReached(void)
{
  for (byte i = 0; i < NUMTUB; i++)
    if (flipIndics[i]) return false;
  return true;
}

/* FM_LIST: каждый меняющийся разряд прокручивает значения по убыванию,
 * пока не достигнет целевого.
 */
static void flipList(void)
{
  if (!flipInit)
  {
    flipInit = true;
    flipTimer.setInterval(flip_speed[flip_effect]);
    markChangedDigits();
  }
  if (!flipTimer.isReady()) return;

  for (byte i = 0; i < NUMTUB; i++)
  {
    if (!flipIndics[i]) continue;
    indiDigits[i]--;
    if (indiDigits[i] < 0) indiDigits[i] = 9;
    if (indiDigits[i] == newTime[i]) flipIndics[i] = false;
  }
  if (allDigitsReached())
  {
    flipInit = false;
    newTimeFlag = false;
    newSecFlag = false;
  }
}

/* FM_CATHODE: каждый разряд проходит по порядку расположения катодов
 * (см. cathodeMask), пока не достигнет целевой цифры по кратчайшему пути.
 */
static void flipCathode(void)
{
  if (!flipInit)
  {
    flipInit = true;
    flipTimer.setInterval(flip_speed[flip_effect]);
    markChangedDigits();
    // запомнить стартовую/конечную позицию катода для каждой меняющейся цифры
    for (byte i = 0; i < NUMTUB; i++)
    {
      if (!flipIndics[i]) continue;
      for (byte c = 0; c < 10; c++)
      {
        if (cathodeMask[c] == indiDigits[i]) startCathode[i] = c;
        if (cathodeMask[c] == newTime[i])    endCathode[i]   = c;
      }
    }
  }
  if (!flipTimer.isReady()) return;

  for (byte i = 0; i < NUMTUB; i++)
  {
    if (!flipIndics[i]) continue;
    if (startCathode[i] > endCathode[i])      indiDigits[i] = cathodeMask[--startCathode[i]];
    else if (startCathode[i] < endCathode[i]) indiDigits[i] = cathodeMask[++startCathode[i]];
    else                                       flipIndics[i] = false;
  }
  if (allDigitsReached())
  {
    flipInit = false;
    newTimeFlag = false;
    newSecFlag = false;
  }
}

/* FM_TRAIN: «уезжает» старое значение, а на его место «въезжает» новое. */
static void flipTrain(void)
{
  if (!newTimeFlag)
  {
    if (newSecFlag) refreshSecondsDigits();
    return;
  }

  if (!flipInit)
  {
    flipInit = true;
    currentLamp = 0;
    trainLeaving = true;
    flipTimer.setInterval(flip_speed[flip_effect]);
  }
  if (!flipTimer.isReady()) return;

  if (trainLeaving)
  { // сдвигаем разряды и гасим освобождающийся
    for (byte i = NUMTUB - 1; i > currentLamp; i--)
      indiDigits[i] = indiDigits[i - 1];
    anodeStates &= ~(1 << currentLamp);
    if (++currentLamp >= NUMTUB)
    {
      trainLeaving = false;
      currentLamp = 0;
    }
  }
  else
  { // в обратную сторону — въезжает новое
    for (byte i = currentLamp; i > 0; i--)
      indiDigits[i] = indiDigits[i - 1];
    indiDigits[0] = newTime[NUMTUB - 1 - currentLamp];
    anodeStates |= 1 << currentLamp;
    if (++currentLamp >= NUMTUB)
    {
      flipInit = false;
      newTimeFlag = false;
    }
  }
}

/* FM_ELASTIC: «резинка» — цифры собираются справа, формируют новое число
 * слева и тянутся обратно. Сценарий описан кадрами flipEffectStages 0..43.
 *
 * Каждый кадр устанавливает биты anodeStates и переставляет цифры в indiDigits;
 * поведение нерегулярное — table-driven вариант был бы крупнее и менее наглядным.
 */
static void flipElastic(void)
{
  if (!newTimeFlag)
  {
    if (newSecFlag) refreshSecondsDigits();
    return;
  }

  if (!flipInit)
  {
    flipInit = true;
    flipEffectStages = 0;
    flipTimer.setInterval(flip_speed[flip_effect]);
  }
  if (!flipTimer.isReady()) return;

  switch (flipEffectStages++)
  {
  // фаза стягивания цифр влево
  case 1: case 3: case 6: case 10: case 15: case 21:
    anodeStates &= ~(1 << 5);
    break;
  case 2: case 5: case 9: case 14: case 20:
    anodeStates &= ~(1 << 4);
    indiDigits[5] = indiDigits[4];
    anodeStates |=  (1 << 5);
    break;
  case 4: case 8: case 13: case 19:
    anodeStates &= ~(1 << 3);
    indiDigits[4] = indiDigits[3];
    anodeStates |=  (1 << 4);
    break;
  case 7: case 12: case 18:
    anodeStates &= ~(1 << 2);
    indiDigits[3] = indiDigits[2];
    anodeStates |=  (1 << 3);
    break;
  case 11: case 17:
    anodeStates &= ~(1 << 1);
    indiDigits[2] = indiDigits[1];
    anodeStates |=  (1 << 2);
    break;
  case 16:
    anodeStates &= ~(1 << 0);
    indiDigits[1] = indiDigits[0];
    anodeStates |=  (1 << 1);
    break;
  case 22:
    indiDigits[0] = newTime[5];
    anodeStates |= (1 << 0);
    break;
  // фаза разъезжания новых цифр вправо
  case 23: case 29: case 34: case 38: case 41:
    anodeStates &= ~(1 << 0);
    indiDigits[1] = indiDigits[0];
    anodeStates |=  (1 << 1);
    break;
  case 24: case 30: case 35: case 39:
    anodeStates &= ~(1 << 1);
    indiDigits[2] = indiDigits[1];
    anodeStates |=  (1 << 2);
    break;
  case 25: case 31: case 36:
    anodeStates &= ~(1 << 2);
    indiDigits[3] = indiDigits[2];
    anodeStates |=  (1 << 3);
    break;
  case 26: case 32:
    anodeStates &= ~(1 << 3);
    indiDigits[4] = indiDigits[3];
    anodeStates |=  (1 << 4);
    break;
  case 27:
    anodeStates &= ~(1 << 4);
    indiDigits[5] = indiDigits[4];
    anodeStates |=  (1 << 5);
    break;
  case 28: indiDigits[0] = newTime[4]; anodeStates |= (1 << 0); break;
  case 33: indiDigits[0] = newTime[3]; anodeStates |= (1 << 0); break;
  case 37: indiDigits[0] = newTime[2]; anodeStates |= (1 << 0); break;
  case 40: indiDigits[0] = newTime[1]; anodeStates |= (1 << 0); break;
  case 42: indiDigits[0] = newTime[0]; anodeStates |= (1 << 0); break;
  case 43:
    flipInit = false;
    newTimeFlag = false;
    break;
  }
}

/* Диспетчер эффекта перелистывания. */
void flipTick(void)
{
  switch (flip_effect)
  {
  case FM_NULL:    flipNull();    break;
  case FM_SMOOTH:  flipSmooth();  break;
  case FM_LIST:    flipList();    break;
  case FM_CATHODE: flipCathode(); break;
  case FM_TRAIN:   flipTrain();   break;
  case FM_ELASTIC: flipElastic(); break;
  }
}
