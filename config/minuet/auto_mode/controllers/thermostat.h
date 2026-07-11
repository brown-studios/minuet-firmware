// MINUET AUTO MODE THERMOSTAT HEADER

#pragma once

#include "common.h"
#include "sensors.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

enum class ThermostatStep : uint8_t {
  STEP_1_F = 0,
  STEP_0_5_C = 1,
};

ThermostatStep get_thermostat_step() {
  return ThermostatStep(minuet_auto_thermostat_step->active_index().value_or(0));
}

bool get_thermostat_default_on() {
  return (MINUET_AUTO_THERMOSTAT_DEFAULT_ON);
}

float get_thermostat_default_target_temperature() {
  switch (get_thermostat_step()) {
    default:
    case ThermostatStep::STEP_1_F:
      return esphome::fahrenheit_to_celsius(float(MINUET_AUTO_THERMOSTAT_DEFAULT_TARGET_TEMPERATURE_F));
    case ThermostatStep::STEP_0_5_C:
      return float(MINUET_AUTO_THERMOSTAT_DEFAULT_TARGET_TEMPERATURE_C);
  }
}

AutoFanMode get_thermostat_default_fan_mode() {
  return AutoFanMode::MINUET_AUTO_THERMOSTAT_DEFAULT_FAN_MODE;
}

int get_thermostat_default_fan_speed() {
  return (MINUET_AUTO_THERMOSTAT_DEFAULT_FAN_SPEED);
}

int get_thermostat_fan_speed() {
  return int(minuet_auto_thermostat_fan_speed->state);
}

void set_thermostat_fan_speed(int fan_speed) {
  minuet_auto_thermostat_fan_speed->make_call().set_value(fan_speed).perform();
}

class Thermostat final : public Controller {
 public:
  Demand prepare(uint64_t now_ms, bool run) override;
  void apply(const Output &output) override;

 private:
  enum class Status : uint8_t {
    OFF = 0,
    IDLE,
    DEMAND,
  };
  static constexpr const char *STATUS_STRINGS[] = {"OFF", "IDLE", "DEMAND"};

  Status status_{};
  float current_temperature_{NAN};
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
  float current_humidity_{NAN};
#endif
};

Demand Thermostat::prepare(uint64_t now_ms, bool run) {
  const bool on = minuet_auto_thermostat->mode == esphome::climate::CLIMATE_MODE_COOL;
  const float target_temperature = minuet_auto_thermostat->target_temperature;
  const float hysteresis_upper = minuet_auto_thermostat_hysteresis_upper->state;
  const float hysteresis_lower = minuet_auto_thermostat_hysteresis_lower->state;
  const AutoFanMode fan_mode = get_climate_fan_mode(minuet_auto_thermostat);
  const int fan_speed = get_thermostat_fan_speed();
  const float dynamic_range = minuet_auto_thermostat_dynamic_range->state;

  this->current_temperature_ = sensors.temperature.get_state();
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
  this->current_humidity_ = sensors.humidity.get_state();
#endif
  if (!run || !on || std::isnan(this->current_temperature_)) {
    this->status_ = Status::OFF;
    return {};
  }

  const float delta = this->current_temperature_ - target_temperature;
  bool active;
  if (-delta >= hysteresis_lower) {
    active = false;
  } else if (delta >= hysteresis_upper) {
    active = true;
  } else {
    active = this->status_ == Status::DEMAND;
  }
  if (!active) {
    this->status_ = Status::IDLE;
    return {};
  }
  this->status_ = Status::DEMAND;

  // TODO: It might be useful to add a fan mode that also considers the current humidity to increase
  // airflow in higher humidity environments where evaporative cooling will be less effective.
  switch (fan_mode) {
    case AutoFanMode::AUTO:
      if (dynamic_range <= 0 || fan_speed <= 0) {
        return {.minimum_fan_speed = fan_speed};
      }
      return {.minimum_fan_speed = std::clamp(int(fan_speed * std::clamp(delta / dynamic_range, 0.f, 1.f)), 1, 10)};
    case AutoFanMode::ON:
      return {.minimum_fan_speed = fan_speed};
    default:
    case AutoFanMode::OFF:
      return {.minimum_fan_speed = 0};
  }
}

void Thermostat::apply(const Output &output) {
  MINUET_LOGD_IF_CHANGED(100,
      "Auto thermostat: status %s, %.1f °C, "
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
      "%.1f %%RH",
#endif
      STATUS_STRINGS[size_t(this->status_)], this->current_temperature_,
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
      this->current_humidity_
#endif
  );

  const auto action = this->status_ == Status::OFF            ? esphome::climate::CLIMATE_ACTION_OFF
      : this->status_ >= Status::DEMAND && output.is_active() ? esphome::climate::CLIMATE_ACTION_FAN
                                                              : esphome::climate::CLIMATE_ACTION_IDLE;
  if (!equal_or_both_nan(minuet_auto_thermostat->current_temperature, this->current_temperature_) ||
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
      !equal_or_both_nan(minuet_auto_thermostat->current_humidity, this->current_humidity_) ||
#endif
      minuet_auto_thermostat->action != action) {
    minuet_auto_thermostat->current_temperature = this->current_temperature_;
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
    minuet_auto_thermostat->current_humidity = this->current_humidity_;
#endif
    minuet_auto_thermostat->action = action;
    minuet_auto_thermostat->publish_state();
  }
}

}  // namespace auto_mode
}  // namespace minuet
