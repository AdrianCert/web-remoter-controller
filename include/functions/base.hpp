#pragma once
#include "Arduino.h"

class BaseFunction {

public:
  virtual String name();
  virtual bool run(String cmd, uint32_t req_id = 0);
};