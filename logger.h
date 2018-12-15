#ifndef LOGGER_H
#define LOGGER_H
#include <iostream>
#include <pin.H>

enum LevelDebug { _ERROR = 0, _WARNING, _INFO, _DEBUG, _ALL_LOG };

class Log {
  static LevelDebug gLevel;

  LevelDebug cLevel;

public:
  Log(const LevelDebug level, const std::string &fileName,
      const std::string &funcName, const int line) {
    cLevel = level;
    if (cLevel <= gLevel)
      std::cout << fileName << "/" << funcName << ":" << line << ": ";
  }

  template <class T> Log &operator<<(const T &v) {
    if (cLevel <= gLevel)
      std::cout << v;
    return *this;
  }

  ~Log() {
    if (cLevel <= gLevel)
      std::cout << std::endl;
  }

  static void setGLevel(const LevelDebug level) { gLevel = level; }
};

#define MAGIC_LOG(LEVEL) Log((LEVEL), __FILE__, __FUNCTION__, __LINE__)

#endif // LOGGER_H
