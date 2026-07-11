// MINUET AUTO MODE AIR QUALITY MONITOR HEADER

#pragma once

#include "common.h"
#include "sensors.h"

namespace minuet {
namespace auto_mode {

class AirQualityMonitor final : public Controller {
 public:
  Demand prepare(uint64_t now, bool run) override;
  void apply(const Output &output) override;
};

Demand AirQualityMonitor::prepare(uint64_t now, bool run) {
  return {};
}

void AirQualityMonitor::apply(const Output &output) {
}

}  // namespace auto_mode
}  // namespace minuet
