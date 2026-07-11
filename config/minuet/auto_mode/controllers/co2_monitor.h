// MINUET AUTO MODE CO2 MONITOR HEADER

#pragma once

#include "common.h"
#include "sensors.h"

namespace minuet {
namespace auto_mode {

class CO2Monitor final : public Controller {
 public:
  Demand prepare(uint64_t now, bool run) override;
  void apply(const Output &output) override;
};

Demand CO2Monitor::prepare(uint64_t now, bool run) {
  return {};
}

void CO2Monitor::apply(const Output &output) {
}

}  // namespace auto_mode
}  // namespace minuet
