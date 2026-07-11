// MINUET AUTO MODE SENSORS HEADER

#pragma once

#include <cstdint>
#include <initializer_list>
#include <utility>

#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

void schedule_governor_update();

struct SensorSource {
  SensorSource(esphome::sensor::Sensor *sensor) : type(Type::SENSOR), sensor(sensor) {
  }
  SensorSource(esphome::number::Number *number) : type(Type::NUMBER), number(number) {
  }

  enum class Type {
    SENSOR = 0,
    NUMBER,
  };

  const Type type;
  union {
    esphome::sensor::Sensor *const sensor;
    esphome::number::Number *const number;
  };

  float get_state() const {
    switch (type) {
      case Type::SENSOR:
        if (sensor->has_state())
          return sensor->state;
        break;
      case Type::NUMBER:
        return number->state;
    }
    return NAN;
  }

  template<typename F> void add_on_state_callback(F &&callback) const {
    switch (type) {
      case Type::SENSOR:
        sensor->add_on_state_callback(std::move(callback));
        break;
      case Type::NUMBER:
        number->add_on_state_callback(std::move(callback));
        break;
    }
  }
};

struct SensorOption {
  SensorSource source;
  float priority;

  bool is_debug() const {
    return priority < 0;
  }
};

using SensorOptions = esphome::FixedVector<SensorOption>;

SensorOptions make_sensor_options(
    std::initializer_list<SensorSource> sources, std::initializer_list<float> priorities) {
  ESPHOME_DEBUG_ASSERT(sources.size() == priorities.size());

  SensorOptions options;
  options.init(sources.size());

  auto source_it = sources.begin();
  auto priority_it = priorities.begin();
  while (source_it != sources.end()) {
    options.push_back({*source_it, *priority_it});
    ++source_it;
    ++priority_it;
  }
  return options;
}

// Uses a Select entity to choose which Sensor to observe among an ordered list of options.
// If the selected index is zero, finds the first Sensor with a valid state and provides its value.
// Otherwise, provides the state of the (index-1)th Sensor.
// Invokes a callback whenever the state changes.
class SelectableSensor {
 public:
  void init(esphome::select::Select *select, SensorOptions options) {
    this->select_ = select;
    this->options_ = std::move(options);

    this->select_->add_on_state_callback([this](size_t) { this->update_(); });
    for (const auto &option : this->options_) {
      option.source.add_on_state_callback([this](float) { this->update_(); });
    }
    this->update_();
  }

  float get_state() const {
    return this->state_;
  }

 private:
  void update_() {
    float state = NAN;
    size_t index = this->select_->active_index().value_or(0);
    if (index == 0) {
      for (const auto &option : this->options_) {
        if (!option.is_debug()) {
          state = option.source.get_state();
          if (!std::isnan(state)) {
            break;
          }
        }
      }
    } else {
      index -= 1;
      if (index < this->options_.size()) {
        state = this->options_[index].source.get_state();
      }
    }
    if (!equal_or_both_nan(this->state_, state)) {
      this->state_ = state;
      schedule_governor_update();
    }
  }

  esphome::select::Select *select_{};
  SensorOptions options_{};
  float state_{NAN};
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

  void init();
};

Sensors sensors;

void Sensors::init() {
#ifdef MINUET_AUTO_SENSOR_TEMPERATURE
  this->temperature.init(minuet_auto_sensor_temperature,
      make_sensor_options({MINUET_AUTO_SENSOR_TEMPERATURE}, {MINUET_AUTO_SENSOR_TEMPERATURE_PRIORITY}));
#endif
#ifdef MINUET_AUTO_SENSOR_HUMIDITY
  this->humidity.init(minuet_auto_sensor_humidity,
      make_sensor_options({MINUET_AUTO_SENSOR_HUMIDITY}, {MINUET_AUTO_SENSOR_HUMIDITY_PRIORITY}));
#endif
#ifdef MINUET_AUTO_SENSOR_CO2
  this->co2.init(
      minuet_auto_sensor_co2, make_sensor_options({MINUET_AUTO_SENSOR_CO2}, {MINUET_AUTO_SENSOR_CO2_PRIORITY}));
#endif
#ifdef MINUET_AUTO_SENSOR_AQI
  this->aqi.init(
      minuet_auto_sensor_aqi, make_sensor_options({MINUET_AUTO_SENSOR_AQI}, {MINUET_AUTO_SENSOR_AQI_PRIORITY}));
#endif
}

}  // namespace auto_mode
}  // namespace minuet
