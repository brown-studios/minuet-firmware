// MINUET AUTO MODE CONTROLLERS HEADER

#pragma once

#include "common.h"
#include "esphome/core/helpers.h"

#ifdef MINUET_AUTO_THERMOSTAT
#include "thermostat.h"
#endif
#ifdef MINUET_AUTO_HUMIDISTAT
#include "humidistat.h"
#endif
#ifdef MINUET_AUTO_CO2_MONITOR
#include "co2_monitor.h"
#endif
#ifdef MINUET_AUTO_AIR_QUALITY_MONITOR
#include "air_quality_monitor.h"
#endif

namespace minuet {
namespace auto_mode {

esphome::FixedVector<Controller *> make_controllers() {
  return {
#ifdef MINUET_AUTO_THERMOSTAT
      new Thermostat(),
#endif
#ifdef MINUET_AUTO_HUMIDISTAT
      new Humidistat(),
#endif
#ifdef MINUET_AUTO_CO2_MONITOR
      new CO2Monitor(),
#endif
#ifdef MINUET_AUTO_AIR_QUALITY_MONITOR
      new AirQualityMonitor(),
#endif
  };
}

}  // namespace auto_mode
}  // namespace minuet
