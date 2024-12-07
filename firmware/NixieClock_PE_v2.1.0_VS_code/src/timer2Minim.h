#ifndef TIMER_MINIM_H
#define TIMER_MINIM_H
// мини-класс таймера, версия 2.0
// использован улучшенный алгоритм таймера на millis
// алгоритм чуть медленнее, но обеспечивает кратные интервалы и защиту от пропусков и переполнений

class timerMinim
{
  public:
    // объявление таймера с указанием интервала
    timerMinim(uint32_t interval)
    {
      _interval = (interval != 0) ? interval : 5;
      _timer = millis();
    };				            

    // установка интервала работы таймера
    void setInterval(uint32_t interval)
    {
        _interval = (interval != 0) ? interval : 5;
    }

    // возвращает true, когда пришло время. Сбрасывается в false сам (AUTO) или вручную (MANUAL)	            
    boolean isReady()
    {
      uint32_t thisMls = millis();
      if (thisMls - _timer >= _interval) 
      {
        do 
        {
          _timer += _interval;
          if (_timer < _interval) break;                  // переполнение uint32_t
        } while (_timer < thisMls - _interval);           // защита от пропуска шага
        return true;
      } 
      else 
      {
        return false;
      }
    }

    // возвращает true, когда пришло время. Сбрасывается после вызова						                    
    boolean isReadyDisable()
    {
      if (_enabled)
      {
        uint32_t thisMls = millis();
        if (thisMls - _timer >= _interval) 
        {
          do {
            _timer += _interval;
            if (_timer < _interval) break;                  // переполнение uint32_t
          } while (_timer < thisMls - _interval);           // защита от пропуска шага
          _enabled = false;
          return true;
        } 
        else 
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }

    // ручной сброс таймера на установленный интервал
    void reset()
    {
      _timer = millis();
      _enabled = true;
    }							                        
    
    // выключение таймера
    void disable() 
    {
      _enabled = false;
    };							                      

  private:
    uint32_t _timer = 0;
    uint32_t _interval = 0;
    boolean _enabled = false;
};


#endif