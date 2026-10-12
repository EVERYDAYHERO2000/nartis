// Firmware port shim: minimal ESPHome Component/PollingComponent.
// The firmware task drives setup()/loop()/update() and run_pending_().
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "esphome/core/hal.h"
namespace esphome {
namespace setup_priority {
constexpr float BUS = 1000.0f, IO = 900.0f, HARDWARE = 800.0f, DATA = 600.0f,
                PROCESSOR = 400.0f, AFTER_WIFI = 250.0f, LATE = -100.0f;
}
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return setup_priority::DATA; }
  void set_timeout(uint32_t ms, std::function<void()> &&f) { pending_.push_back({millis() + ms, std::move(f)}); }
  void set_timeout(const std::string &, uint32_t ms, std::function<void()> &&f) { set_timeout(ms, std::move(f)); }
  void defer(std::function<void()> &&f) { set_timeout(0, std::move(f)); }
  void defer(const std::string &, std::function<void()> &&f) { set_timeout(0, std::move(f)); }
  void mark_failed() { failed_ = true; }
  bool is_failed() const { return failed_; }
  // Run callbacks whose time has come (callbacks may schedule new ones).
  void run_pending_() {
    if (pending_.empty()) return;
    const uint32_t now = millis();
    std::vector<std::function<void()>> due;
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (static_cast<int32_t>(now - it->due) >= 0) { due.push_back(std::move(it->fn)); it = pending_.erase(it); }
      else ++it;
    }
    for (auto &f : due) f();
  }
 protected:
  struct Pending { uint32_t due; std::function<void()> fn; };
  std::vector<Pending> pending_;
  bool failed_{false};
};
class PollingComponent : public Component {
 public:
  PollingComponent() = default;
  explicit PollingComponent(uint32_t interval) : update_interval_(interval) {}
  virtual void update() = 0;
  void set_update_interval(uint32_t ms) { update_interval_ = ms; }
  uint32_t get_update_interval() const { return update_interval_; }
 protected:
  uint32_t update_interval_{600000};
};
}  // namespace esphome
