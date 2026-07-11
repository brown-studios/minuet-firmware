// MINUET UTILITY HEADER
//
// Provides helper functions.

#pragma once

#include <cstdint>
#include <cmath>
#include <optional>

#include "esphome/core/application.h"

namespace minuet {

const char *const TAG = "minuet";

bool equal_or_both_nan(float a, float b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}

struct MinCriterion {
  static inline bool compare(float value, float accumulator) {
    return value < accumulator;
  }
};

struct MaxCriterion {
  static inline bool compare(float value, float accumulator) {
    return value > accumulator;
  }
};

template<typename Criterion> struct ThrottleFilterWithCriterion {
  float accumulator{NAN};
  uint32_t time{0};

  std::optional<float> operator()(float value, uint32_t throttle_ms) {
    const uint32_t now = esphome::App.get_loop_component_start_time();
    if (!std::isnan(value)) {
      if (std::isnan(this->accumulator) || Criterion::compare(value, this->accumulator)) {
        this->accumulator = value;
      }
      if (now - this->time < throttle_ms) {
        return {};
      }
    }
    time = now;
    if (!std::isnan(this->accumulator)) {
      const float result = this->accumulator;
      accumulator = NAN;
      return result;
    }
    return {};
  }
};

}  // namespace minuet
