// MINUET AUTO MODE SENSORS HEADER
//
// Factored out to a separate header to encourage layering.

#pragma once

#include <cstdint>
#include <optional>

#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

using SensorOptions = esphome::FixedVector<esphome::sensor::Sensor *>;

template<typename... TOptions> SensorOptions make_sensor_options(TOptions... options...) {
  return {std::forward<TOptions>(options)...};
}

// Uses a Select entity to choose which Sensor to observe among an ordered list of options.
// If the selected index is zero, finds the first Sensor with a valid state and provides its value.
// Otherwise, provides the state of the (index-1)th Sensor.
// Invokes a callback whenever the state changes.
class SelectableSensor {
 public:
  void init(esphome::select::Select *select, SensorOptions sensors) {
    this->select_ = select;
    this->sensors_ = std::move(sensors);

    this->select_->add_on_state_callback([this](size_t) { this->update_(); });
    for (const auto &sensor : this->sensors_) {
      sensor->add_on_state_callback([this](float) { this->update_(); });
    }
    this->update_();
  }

  std::optional<float> get_state() const {
    return this->state_;
  }

  esphome::CallbackManager<void()> on_state;

 private:
  void update_() {
    std::optional<float> state{};
    size_t index = this->select_->active_index().value_or(0);
    if (index == 0) {
      for (const auto &sensor : this->sensors_) {
        if (sensor->has_state() && !std::isnan(sensor->state)) {
          state = sensor->state;
          break;
        }
      }
    } else {
      index -= 1;
      if (index < this->sensors_.size() && this->sensors_[index]->has_state() &&
          !std::isnan(this->sensors_[index]->state)) {
        state = this->sensors_[index]->state;
      }
    }
    if (this->state_ != state) {
      this->state_ = state;
      this->on_state.call();
    }
  }

  esphome::select::Select *select_{};
  SensorOptions sensors_{};
  std::optional<float> state_{};
};

struct Sensors {
  SelectableSensor temperature{};
  SelectableSensor humidity{};
  SelectableSensor co2{};
  SelectableSensor aqi{};
  SelectableSensor caqi{};
};
Sensors sensors;

void init_sensors(void (*state_callback)()) {
  sensors.temperature.init(minuet_auto_sensor_temperature, make_sensor_options(MINUET_AUTO_SENSORS_TEMPERATURE));
  sensors.temperature.on_state.add(state_callback);

  sensors.humidity.init(minuet_auto_sensor_humidity, make_sensor_options(MINUET_AUTO_SENSORS_HUMIDITY));
  sensors.humidity.on_state.add(state_callback);

  sensors.co2.init(minuet_auto_sensor_co2, make_sensor_options(MINUET_AUTO_SENSORS_CO2));
  sensors.co2.on_state.add(state_callback);

  sensors.aqi.init(minuet_auto_sensor_aqi, make_sensor_options(MINUET_AUTO_SENSORS_AQI));
  sensors.aqi.on_state.add(state_callback);

  sensors.caqi.init(minuet_auto_sensor_caqi, make_sensor_options(MINUET_AUTO_SENSORS_CAQI));
  sensors.caqi.on_state.add(state_callback);
}

}  // namespace auto_mode
}  // namespace minuet
