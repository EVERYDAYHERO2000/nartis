// Firmware port shim: text sensor.
#pragma once
#include <string>
#include "esphome/core/component.h"
namespace esphome::text_sensor {
class TextSensor {
 public:
  virtual ~TextSensor() = default;
  virtual void publish_state(const std::string &state) { this->state = state; has_state_ = true; }
  const std::string &get_name() const { return name_; }
  void set_name(const std::string &n) { name_ = n; }
  bool has_state() const { return has_state_; }
  std::string state;
 protected:
  std::string name_;
  bool has_state_{false};
};
}  // namespace esphome::text_sensor
