// мини-класс таймера, версия 2.0
// использован улучшенный алгоритм таймера на millis
// алгоритм чуть медленнее, но обеспечивает кратные интервалы и защиту от пропусков и переполнений

class timerMinim
{
  public:
    timerMinim(uint32_t interval);				            // объявление таймера с указанием интервала
    void setInterval(uint32_t interval);	            // установка интервала работы таймера
    boolean isReady();						                    // возвращает true, когда пришло время. Сбрасывается в false сам (AUTO) или вручную (MANUAL)
    boolean isReadyDisable();						                // возвращает true, когда пришло время. Сбрасывается после вызова
    void reset();							                        // ручной сброс таймера на установленный интервал
    void disable();							                      // выключение таймера

  private:
    uint32_t _timer = 0;
    uint32_t _interval = 0;
    boolean _enabled = false;
};

timerMinim::timerMinim(uint32_t interval) {
  _interval = (interval != 0) ? interval : 5;
  _timer = millis();
}

void timerMinim::setInterval(uint32_t interval) {
  _interval = (interval != 0) ? interval : 5;
  _interval = interval;
}

boolean timerMinim::isReady() {
  uint32_t thisMls = millis();
  if (thisMls - _timer >= _interval) {
    do {
      _timer += _interval;
      if (_timer < _interval) break;                  // переполнение uint32_t
    } while (_timer < thisMls - _interval);           // защита от пропуска шага
    return true;
  } else {
    return false;
  }
}

boolean timerMinim::isReadyDisable() {
  if (_enabled)
  {
    uint32_t thisMls = millis();
    if (thisMls - _timer >= _interval) {
      do {
        _timer += _interval;
        if (_timer < _interval) break;                  // переполнение uint32_t
      } while (_timer < thisMls - _interval);           // защита от пропуска шага
      _enabled = false;
      return true;
    } else {
      return false;
    }
  }
  else
  {
    return false;
  }

}

void timerMinim::reset() {
  _timer = millis();
  _enabled = true;
}

void timerMinim::disable() {
  _enabled = false;
}