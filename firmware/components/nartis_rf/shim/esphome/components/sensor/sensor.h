// Firmware port shim: numeric sensor; the firmware subclasses publish_state().
#pragma once
#include <cmath>
#include <string>
#include "esphome/core/component.h"
namespace esphome::sensor {
class Sensor {
 public:
  virtual ~Sensor() = default;
  virtual void publish_state(float state) { this->state = state; has_state_ = true; }
  const std::string &get_name() const { return name_; }
  void set_name(const std::string &n) { name_ = n; }
  bool has_state() const { return has_state_; }
  float state{NAN};
 protected:
  std::string name_;
  bool has_state_{false};
};
}  // namespace esphome::sensor
