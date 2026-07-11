// MINUET AUTO MODE HUMIDISTAT HEADER

#pragma once

#include "common.h"
#include "sensors.h"

namespace minuet {
namespace auto_mode {

class Humidistat final : public Controller {
 public:
  Demand prepare(uint64_t now, bool run) override;
  void apply(const Output &output) override;
};

Demand Humidistat::prepare(uint64_t now, bool run) {
  return {};
}

void Humidistat::apply(const Output &output) {
}

}  // namespace auto_mode
}  // namespace minuet
