// MINUET AUTO MODE CONTROLLERS HEADER
//
// Implements the thermostat, humidistat, CO2 monitor, and air quality monitor controllers.
// These classes should only access their sensors and settings and have no other side-effects.
// Factored out to a separate header to encourage layering.

#pragma once

#include <cstdint>
#include <optional>

#include "auto_mode_sensors.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

constexpr const float AUTO_THERMOSTAT_TARGET_TEMPERATURE_DEFAULT =
    (MINUET_AUTO_THERMOSTAT_TARGET_TEMPERATURE_DEFAULT);  // in °C
constexpr const float AUTO_THERMOSTAT_TARGET_TEMPERATURE_STEP =
    (MINUET_AUTO_THERMOSTAT_TARGET_TEMPERATURE_STEP);  // in °C

class Controller {
 public:
  struct Demand {
    // The minimum fan speed demanded by the controller.
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

  virtual void start() {
  }

  virtual void stop() {
  }

  virtual Demand update(bool running) = 0;

  void schedule_update();
};

class Thermostat final : public Controller {
 public:
  Demand update(bool running) override;
};

Thermostat::Demand Thermostat::update(bool running) {
  const auto temperature = sensors.temperature.get_state();
  if (running && temperature.has_value() && temperature.value() > minuet_auto_thermostat->target_temperature) {
    return {.minimum_fan_speed = 5};
  }

  return {};
}

class Humidistat final : public Controller {
 public:
  Demand update(bool running) override;
};

Humidistat::Demand Humidistat::update(bool running) {
  return {};
}

class CO2Monitor final : public Controller {
 public:
  Demand update(bool running) override;
};

CO2Monitor::Demand CO2Monitor::update(bool running) {
  return {};
}

class AirQualityMonitor final : public Controller {
 public:
  Demand update(bool running) override;
};

AirQualityMonitor::Demand AirQualityMonitor::update(bool running) {
  return {};
}

esphome::FixedVector<Controller *> init_controllers() {
  return {
      new Thermostat(),
      new Humidistat(),
      new CO2Monitor(),
      new AirQualityMonitor(),
  };
}

}  // namespace auto_mode
}  // namespace minuet
