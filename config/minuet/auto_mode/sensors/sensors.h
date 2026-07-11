// MINUET AUTO MODE SENSORS HEADER

#pragma once

#include <cstdint>
#include <optional>

#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

void schedule_governor_update();

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
      schedule_governor_update();
    }
  }

  esphome::select::Select *select_{};
  SensorOptions sensors_{};
  std::optional<float> state_{};
};

struct Sensors {
#ifdef MINUET_AUTO_SENSOR_TEMPERATURE
  SelectableSensor temperature{};
#endif
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
  SelectableSensor humidity{};
#endif
#ifdef MINUET_AUTO_SENSOR_CO2
  SelectableSensor co2{};
#endif
#ifdef MINUET_AUTO_SENSOR_AQI
  SelectableSensor aqi{};
#endif
#ifdef MINUET_AUTO_SENSOR_CAQI
  SelectableSensor caqi{};
#endif

  void init();
};

Sensors sensors;

void Sensors::init() {
#ifdef MINUET_AUTO_SENSOR_TEMPERATURE
  this->temperature.init(minuet_auto_sensor_temperature, make_sensor_options(MINUET_AUTO_SENSOR_TEMPERATURE));
#endif
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
  this->humidity.init(minuet_auto_sensor_humidity, make_sensor_options(MINUET_AUTO_SENSOR_HUMIDITY));
#endif
#ifdef MINUET_AUTO_SENSOR_CO2
  this->co2.init(minuet_auto_sensor_co2, make_sensor_options(MINUET_AUTO_SENSOR_CO2));
#endif
#ifdef MINUET_AUTO_SENSOR_AQI
  this->aqi.init(minuet_auto_sensor_aqi, make_sensor_options(MINUET_AUTO_SENSOR_AQI));
#endif
#ifdef MINUET_AUTO_SENSOR_CAQI
  this->caqi.init(minuet_auto_sensor_caqi, make_sensor_options(MINUET_AUTO_SENSOR_CAQI));
#endif
}

}  // namespace auto_mode
}  // namespace minuet
