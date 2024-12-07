
#include "NixieClock_PE_v2.1.0.h"
#include "timer2Minim.h"
#include "global_externs.h"

/* Обеспечение отображения эффектов
 *  Входные параметры: нет
 *  Выходные параметры: нет
 */


static boolean flipInit;
static boolean indiBrightDirection;
static boolean flipIndics[NUMTUB];
static byte startCathode[NUMTUB], endCathode[NUMTUB];
static byte currentLamp, flipEffectStages;
static bool trainLeaving;
static const byte cathodeMask[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};  

 void flipTick(void) 
{
  if (FLIP_EFFECT == FM_NULL) 
  {
    sendTime(hrs, mins, secs);
    newTimeFlag = false;
    newSecFlag = false;
  }
  else if (FLIP_EFFECT == FM_SMOOTH) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (newSecFlag) 
      {                              // если изменились секунды во время эффекта - меняем их отображение
        newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
        indiDigits[4] = (byte)secs / 10;
        indiDigits[5] = (byte)secs % 10;
      }
      if (!flipInit) 
      {
        flipInit = true;
        flipTimer.setInterval((unsigned long)FLIP_SPEED[FLIP_EFFECT]);
        flipTimer.reset();
        indiBrightDirection = false;                 // задаём направление
        // запоминаем, какие цифры поменялись и будем менять их яркость
        for (byte i = 0; i < NUMTUB; i++) 
        {
          if (indiDigits[i] != newTime[i]) flipIndics[i] = true;
          else flipIndics[i] = false;
        }
      }
      if (flipTimer.isReady()) 
      {
        if (!indiBrightDirection) 
        {
          indiBrightCounter--;                        // уменьшаем яркость
          if (indiBrightCounter <= 0) 
          {               // если яркость меньше нуля
            indiBrightDirection = true;               // меняем направление изменения
            indiBrightCounter = 0;                    // обнуляем яркость
            sendTime(hrs, mins, secs);                // меняем цифры
          }
        } 
        else 
        {
          indiBrightCounter++;                        // увеличиваем яркость
          if (indiBrightCounter >= indiMaxBright) {   // достигли предела
            indiBrightDirection = false;              // меняем направление
            indiBrightCounter = indiMaxBright;        // устанавливаем максимум
            // выходим из цикла изменения
            flipInit = false;
            newTimeFlag = false;
          }
        }
                                                      // применяем яркость
        for (byte i = 0; i < NUMTUB; i++) if (flipIndics[i]) indiDimm[i] = indiBrightCounter;
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }
  else if (FLIP_EFFECT == FM_LIST) 
  {
    if (!flipInit) 
    {
      flipInit = true;
      flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
                                                    // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (indiDigits[i] != newTime[i]) 
          flipIndics[i] = true;
        else 
          flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) 
    {
      byte flipCounter = 0;
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (flipIndics[i]) 
        {
          indiDigits[i]--;
          if (indiDigits[i] < 0) indiDigits[i] = 9;
          if (indiDigits[i] == newTime[i]) flipIndics[i] = false;
        } 
        else 
        {
          flipCounter++;                            // счётчик цифр, которые не надо менять
        }
      }
      if (flipCounter == NUMTUB) 
      {                  // если ни одну из 6 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
        newSecFlag = false;
      }
    }
  }
  else if (FLIP_EFFECT == FM_CATHODE) 
  {
    if (!flipInit) 
    {
      flipInit = true;
      flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
                                                    // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (indiDigits[i] != newTime[i]) 
        {
          flipIndics[i] = true;
          for (byte c = 0; c < 10; c++) 
          {
            if (cathodeMask[c] == indiDigits[i]) startCathode[i] = c;
            if (cathodeMask[c] == newTime[i]) endCathode[i] = c;
          }
        }
        else flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) 
    {
      byte flipCounter = 0;
      for (byte i = 0; i < NUMTUB; i++) 
      {
        if (flipIndics[i]) 
        {
          if (startCathode[i] > endCathode[i]) 
          {
            startCathode[i]--;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } 
          else if (startCathode[i] < endCathode[i]) 
          {
            startCathode[i]++;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } 
          else 
          {
            flipIndics[i] = false;
          }
        } 
        else 
        {
          flipCounter++;
        }
      }
      if (flipCounter == NUMTUB) 
      {                  // если ни одну из 6 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
        newSecFlag = false;
      }
    }
  }
// --- train --- //
  else if (FLIP_EFFECT == FM_TRAIN) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (!flipInit) 
      {
        flipInit = true;
        currentLamp = 0;
        trainLeaving = true;
        flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
        //flipTimer.reset();
      }
      if (flipTimer.isReady()) 
      {
        if (trainLeaving) 
        {
          for (byte i = NUMTUB-1; i > currentLamp; i--) 
          {
            indiDigits[i] = indiDigits[i-1];
          }
          anodeStates &= ~(1<<currentLamp);
          currentLamp++;
          if (currentLamp >= NUMTUB) 
          {
            trainLeaving = false;                     //coming
            currentLamp = 0;
          }
        }
        else 
        {                                        //trainLeaving == false
          for (byte i = currentLamp; i > 0; i--) 
          {
            indiDigits[i] = indiDigits[i-1];
          }
          indiDigits[0] = newTime[NUMTUB-1-currentLamp];
          anodeStates |= 1<<currentLamp;
          currentLamp++;
          if (currentLamp >= NUMTUB) 
          {
            flipInit = false;
            newTimeFlag = false;
          }
        }
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }

// --- elastic band --- //
  else if (FLIP_EFFECT == FM_ELASTIC) 
  {
    if (newTimeFlag) 
    {                             // штатная обработка эффекта
      if (!flipInit) 
      {
        flipInit = true;
        flipEffectStages = 0;
        flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
        // flipTimer.reset();
      }
      if (flipTimer.isReady()) 
      {
        switch (flipEffectStages++) 
        {
          case 1:
          case 3:
          case 6:
          case 10:
          case 15:
          case 21:
            anodeStates &= ~(1<<5);
            break;
          case 2:
          case 5:
          case 9:
          case 14:
          case 20:
            anodeStates &= ~(1<<4); indiDigits[5] = indiDigits[4]; anodeStates |= 1<<5;
            break;
          case 4:
          case 8:
          case 13:
          case 19:
            anodeStates &= ~(1<<3); indiDigits[4] = indiDigits[3]; anodeStates |= 1<<4;
            break;
          case 7:
          case 12:
          case 18:
            anodeStates &= ~(1<<2); indiDigits[3] = indiDigits[2]; anodeStates |= 1<<3;
            break;
          case 11:
          case 17:
            anodeStates &= ~(1<<1); indiDigits[2] = indiDigits[1]; anodeStates |= 1<<2;
            break;
          case 16:
            anodeStates &= ~(1<<0); indiDigits[1] = indiDigits[0]; anodeStates |= 1<<1;
            break;
          case 22:
            indiDigits[0] = newTime[5]; anodeStates |= 1<<0;
            break;
          case 23:
          case 29:
          case 34:
          case 38:
          case 41:
            anodeStates &= ~(1<<0); indiDigits[1] = indiDigits[0]; anodeStates |= 1<<1;
            break;
          case 24:
          case 30:
          case 35:
          case 39:
            anodeStates &= ~(1<<1); indiDigits[2] = indiDigits[1]; anodeStates |= 1<<2;
            break;
          case 25:
          case 31:
          case 36:
            anodeStates &= ~(1<<2); indiDigits[3] = indiDigits[2]; anodeStates |= 1<<3;
            break;
          case 26:
          case 32:
            anodeStates &= ~(1<<3); indiDigits[4] = indiDigits[3]; anodeStates |= 1<<4;
            break;
          case 27:
            anodeStates &= ~(1<<4); indiDigits[5] = indiDigits[4]; anodeStates |= 1<<5;
            break;
          case 28:
            indiDigits[0] = newTime[4]; anodeStates |= 1<<0;
            break;
          case 33:
            indiDigits[0] = newTime[3]; anodeStates |= 1<<0;
            break;
          case 37:
            indiDigits[0] = newTime[2]; anodeStates |= 1<<0;
            break;
          case 40:
            indiDigits[0] = newTime[1]; anodeStates |= 1<<0;
            break;        
          case 42:
            indiDigits[0] = newTime[0]; anodeStates |= 1<<0;
            break;
          case 43:
            flipInit = false;
            newTimeFlag = false;
        }
      }
    } 
    else if (newSecFlag) 
    {                        // если минуты не поменялись, но поменялись секунды
      newSecFlag = false;                           // поступаем с секундами также, как для эффекта = 0
      indiDigits[4] = (byte)secs / 10;
      indiDigits[5] = (byte)secs % 10;
    }
  }
}
