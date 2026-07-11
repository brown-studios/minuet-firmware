// MINUET AUTO MODE CONTROLLER HEADER
//
// Each controller implements a specific automatic control algorithm.
// It reads from sensors, determines its desired output state, and provides feedback
// to the user via its climate entity.

#pragma once

#include <cstdint>
#include <optional>

#include "utility.h"

namespace minuet {
namespace auto_mode {

void schedule_governor_update();

enum class AutoFanDirection : uint8_t {
  AUTO = 0,
  AIR_OUT = 1,
  AIR_IN = 2,
};

AutoFanDirection get_auto_fan_direction() {
  return AutoFanDirection(minuet_auto_fan_direction->active_index().value_or(0));
}

void set_auto_fan_direction(AutoFanDirection fan_direction) {
  minuet_auto_fan_direction->make_call().set_index(size_t(fan_direction)).perform();
}

const char *auto_fan_direction_name(AutoFanDirection direction) {
  return minuet_auto_fan_direction->option_at(size_t(direction));
}

enum class AutoLidPosition : uint8_t {
  AUTO = 0,
  OPEN = 1,
  CLOSED = 2,
};

AutoLidPosition get_auto_lid_position() {
  return AutoLidPosition(minuet_auto_lid_position->active_index().value_or(0));
}

void set_auto_lid_position(AutoLidPosition lid_position) {
  minuet_auto_lid_position->make_call().set_index(size_t(lid_position)).perform();
}

const char *auto_lid_position_name(AutoLidPosition lid_position) {
  return minuet_auto_lid_position->option_at(size_t(lid_position));
}

enum class AutoFanMode : uint8_t {
  AUTO = 0,
  ON = 1,
  OFF = 2,
};

const char *auto_fan_mode_name(AutoFanMode fan_mode) {
  const char *NAMES[] = {"Auto", "On", "Off"};
  return NAMES[size_t(fan_mode)];
}

AutoFanMode get_climate_fan_mode(const esphome::climate::Climate *climate) {
  switch (climate->fan_mode.value_or(esphome::climate::CLIMATE_FAN_AUTO)) {
    default:
    case esphome::climate::CLIMATE_FAN_AUTO:
      return AutoFanMode::AUTO;
    case esphome::climate::CLIMATE_FAN_ON:
      return AutoFanMode::ON;
    case esphome::climate::CLIMATE_FAN_OFF:
      return AutoFanMode::OFF;
  }
}

esphome::climate::ClimateCall &set_climate_call_fan_mode(esphome::climate::ClimateCall &call, AutoFanMode fan_mode) {
  switch (fan_mode) {
    default:
    case AutoFanMode::AUTO:
      return call.set_fan_mode(esphome::climate::CLIMATE_FAN_AUTO);
    case AutoFanMode::ON:
      return call.set_fan_mode(esphome::climate::CLIMATE_FAN_ON);
    case AutoFanMode::OFF:
      return call.set_fan_mode(esphome::climate::CLIMATE_FAN_OFF);
  }
}

// A one-shot timer that schedules a governor update when it elapses.
class Timer {
 public:
  ~Timer() {
    this->cancel_();
  }

  // Update the timer expiration, return true if the timer has expired.
  bool update(uint64_t now_ms, uint32_t duration_ms, bool reset) {
    if (reset) {
      this->start_time_ms_ = now_ms;
    }
    uint64_t expiration_time_ms = this->start_time_ms_ + duration_ms;
    if (now_ms >= expiration_time_ms) {
      this->cancel_();
      return true;
    }
    if (expiration_time_ms != this->scheduled_expiration_time_ms_) {
      this->scheduled_expiration_time_ms_ = expiration_time_ms;
      esphome::App.scheduler.set_timeout(this, uint32_t(expiration_time_ms - now_ms), schedule_governor_update);
    }
    return false;
  }

 private:
  void cancel_() {
    if (this->scheduled_expiration_time_ms_) {
      this->scheduled_expiration_time_ms_ = 0;
      esphome::App.scheduler.cancel_timeout(this);
    }
  }

  uint64_t start_time_ms_{};
  uint64_t scheduled_expiration_time_ms_{};
};

// A demand for ventilation from one or more controllers.
// Add up all demands to determine what the fan should be doing.
struct Demand {
  // The minimum fan speed demanded.
  // None means no demand.  Zero means a demand for at least passive ventilation.
  std::optional<int> minimum_fan_speed;

  // A controller may set this to true to block the action of other controllers.
  // For example, if the controller detects that the outside air is too polluted to circulate.
  bool inhibit_ventilation;

  void add(const Demand &other) {
    if (!this->minimum_fan_speed.has_value() ||
        (other.minimum_fan_speed.has_value() && other.minimum_fan_speed.value() > this->minimum_fan_speed.value())) {
      this->minimum_fan_speed = other.minimum_fan_speed;
    }
    this->inhibit_ventilation |= other.inhibit_ventilation;
  }
};

// The output state determined by the governor.
struct Output {
  int fan_speed;
  bool lid_open;

  bool is_active() const {
    return fan_speed != 0 || lid_open;
  }

  bool operator==(const Output &other) const = default;
  bool operator!=(const Output &other) const = default;
};

// Implements a specific control algorithm.
//
// The governor calls `prepare()` on each controller, resolves their demands, then calls
// `apply()` on each controller.
class Controller {
 public:
  // Update the controller's state and ventilation demand.
  virtual Demand prepare(uint64_t now_ms, bool run) = 0;

  // Provide feedback to the user about the controller's effect given the actual output
  // state determined by the governor.
  virtual void apply(const Output &output) = 0;
};

}  // namespace auto_mode
}  // namespace minuet
