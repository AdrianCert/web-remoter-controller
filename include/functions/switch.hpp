#pragma once
#include "base.hpp"

class LocalSwitch : public BaseFunction {
  uint8_t pin_used{0};
  bool _state{false};
  bool _pull_up{false};

  uint8_t get_vtw() {
    if (_pull_up) {
      return _state ? LOW : HIGH;
    }
    return !_state ? LOW : HIGH;
  }

  void update_dev_state() { digitalWrite(this->pin_used, get_vtw()); }

public:
  String name() { return String("LocalSwitch"); }

  LocalSwitch() : BaseFunction() {
    pinMode(pin_used, OUTPUT);
    update_dev_state();
  }

  LocalSwitch(uint8_t pin, bool pullup)
      : BaseFunction(), pin_used(pin), _pull_up(pullup) {
    pinMode(pin_used, OUTPUT);
    update_dev_state();
  }

  void update(bool state) {
    _state = state;
    update_dev_state();
  }

  void toggle() {
    _state = !_state;
    update_dev_state();
  }

  void on() {
    _state = true;
    update_dev_state();
  }

  void off() {
    _state = false;
    update_dev_state();
  }

  bool run(String cmd, uint32_t req_id = 0) {
    uint8_t index{0};
    if (!cmd.startsWith(this->name(), index)) return false;
    index = cmd.indexOf("::", index) + 2;

    if (cmd.startsWith("toggle", index)) {
      toggle();
      return true;
    }

    if (cmd.startsWith("update::on", index)) {
      on();
      return true;
    }

    if (cmd.startsWith("update::off", index)) {
      on();
      return true;
    }

    return false;
  }
};
