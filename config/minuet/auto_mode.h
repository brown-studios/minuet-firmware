// MINUET AUTO MODE HEADER
//
// The auto mode governor continuously evaluates current environmental conditions to determine
// an appropriate fan and lid state to meet the user's stated preferences.

#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "auto_mode_controllers.h"
#include "auto_mode_sensors.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

esphome::climate::ClimateMode get_auto_mode() {
  return minuet_auto_thermostat->mode;
}

bool is_active() {
  return get_auto_mode() != esphome::climate::ClimateMode::CLIMATE_MODE_OFF;
}

enum class AutoFanDirection : uint8_t {
  AUTO = 0,
  AIR_OUT = 1,
  AIR_IN = 2,
};

AutoFanDirection get_auto_fan_direction() {
  return AutoFanDirection(minuet_auto_fan_direction->active_index().value_or(0));
}

void set_auto_fan_direction(AutoFanDirection fan_direction) {
  minuet_auto_fan_direction->make_call().set_index(int(fan_direction)).perform();
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
  minuet_auto_lid_position->make_call().set_index(int(lid_position)).perform();
}

bool is_lid_open_demanded(bool ventilation_demanded) {
  switch (get_auto_lid_position()) {
    default:
    case AutoLidPosition::AUTO:
      return ventilation_demanded;
    case AutoLidPosition::OPEN:
      return true;
    case AutoLidPosition::CLOSED:
      return false;
  }
}

enum class AutoFanMode : uint8_t {
  AUTO = 0,
  QUIET = 1,
  OFF = 2,
  MINIMUM = 3,
  LOW = 4,
  MEDIUM = 5,
  HIGH = 6,
};

esphome::climate::ClimateCall &set_auto_thermostat_call_fan_mode(
    esphome::climate::ClimateCall &call, AutoFanMode fan_mode) {
  switch (fan_mode) {
    default:
    case AutoFanMode::AUTO:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_AUTO);
    case AutoFanMode::QUIET:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_QUIET);
    case AutoFanMode::OFF:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_OFF);
    case AutoFanMode::MINIMUM:
      return call.set_fan_mode("Minimum");
    case AutoFanMode::LOW:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_LOW);
    case AutoFanMode::MEDIUM:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_MEDIUM);
    case AutoFanMode::HIGH:
      return call.set_fan_mode(esphome::climate::ClimateFanMode::CLIMATE_FAN_HIGH);
  }
}

void apply_preset(esphome::climate::ClimatePreset preset) {
  switch (preset) {
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_HOME:
      minuet_auto_preset_home_apply->execute();
      break;
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_SLEEP:
      minuet_auto_preset_sleep_apply->execute();
      break;
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_AWAY:
      minuet_auto_preset_away_apply->execute();
      break;
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_ECO:
      minuet_auto_preset_eco_apply->execute();
      break;
    default:
      break;
  }
}

class Governor {
 public:
  // Called by the coordinator to start the automatic control loop.
  // The governor invokes the provided callback to when the next output state is available.
  using CoordinatorCallback = void (*)();
  void start(CoordinatorCallback callback);

  // Called by the coordinator to stop the automatic control loop.
  void stop();

  // Called by the coordinator while the automatic control loop is running to get the requested output state.
  struct Output {
    int fan_speed;
    bool lid_open;

    bool operator==(const Output &other) const = default;
    bool operator!=(const Output &other) const = default;
  };

  Output get_output() const {
    return this->output_;
  }

  void init(esphome::FixedVector<Controller *> controllers) {
    this->controllers_ = std::move(controllers);
  }

  void update();

 private:
  void update_output_(bool notify_coordinator);

  CoordinatorCallback coordinator_callback_{};
  Output output_{};
  esphome::FixedVector<Controller *> controllers_;
};

void Governor::start(CoordinatorCallback callback) {
  ESP_LOGI(minuet::TAG, "Auto mode governor started");
  assert(callback);
  assert(!this->coordinator_callback_);

  this->coordinator_callback_ = callback;
  for (auto &controller : this->controllers_) {
    controller->start();
  }

  this->update_output_(/*notify_coordinator*/ false);
}

void Governor::stop() {
  ESP_LOGI(minuet::TAG, "Auto mode governor stopped");
  assert(this->coordinator_callback_);

  for (auto &controller : this->controllers_) {
    controller->stop();
  }

  this->coordinator_callback_ = nullptr;
  this->output_ = {};
}

void Governor::update() {
  this->update_output_(/*notify_coordinator*/ true);
}

void Governor::update_output_(bool notify_coordinator) {
  const bool running = !!this->coordinator_callback_;

  // Run the controller update functions even when auto mode is disabled so that the controllers
  // can continue to observe the sensors for hysteresis.
  Controller::Demand demand{};
  for (auto &controller : this->controllers_) {
    demand.add(controller->update(running));
  }

  const bool ventilation_demanded = demand.minimum_fan_speed.has_value() && !demand.inhibit_ventilation;
  const float temperature = sensors.temperature.get_state().value_or(NAN);
  const float humidity = sensors.humidity.get_state().value_or(NAN);
  const ClimateAction action = this->coordinator_callback_
      ? ventilation_demanded ? esphome::climate::CLIMATE_ACTION_FAN : esphome::climate::CLIMATE_ACTION_IDLE
      : esphome::climate::CLIMATE_ACTION_OFF;

  if (!equal_or_both_nan(minuet_auto_thermostat->current_temperature, temperature) ||
      !equal_or_both_nan(minuet_auto_thermostat->current_humidity, humidity) ||
      minuet_auto_thermostat->action != action) {
    minuet_auto_thermostat->current_temperature = temperature;
    minuet_auto_thermostat->current_humidity = humidity;
    minuet_auto_thermostat->action = action;
    minuet_auto_thermostat->publish_state();
  }

  if (running) {
    Output output{
        .fan_speed = demand.minimum_fan_speed.value_or(0),
        .lid_open = is_lid_open_demanded(ventilation_demanded),
    };
    if (this->output_ != output) {
      this->output_ = output;
      if (notify_coordinator) {
        this->coordinator_callback_();
      }
    }
  }
}

Governor governor{};

void schedule_governor_update() {
  esphome::App.scheduler.set_timeout(nullptr, "governor update", 0, [] { governor.update(); });
}

void Controller::schedule_update() {
  schedule_governor_update();
}

void init() {
  init_sensors(schedule_governor_update);
  governor.init(init_controllers());
}

}  // namespace auto_mode
}  // namespace minuet
