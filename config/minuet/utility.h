// MINUET UTILITY HEADER
//
// Provides helper functions.

#pragma once

#include <cstdint>
#include <cmath>
#include <optional>
#include <string>

#include "esphome/core/application.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace minuet {

const char *const TAG = "minuet";

bool equal_or_both_nan(float a, float b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}

void publish_state_if_changed(esphome::text_sensor::TextSensor *entity, const std::string &state) {
  if (entity->get_raw_state() != state) {
    entity->publish_state(state);
  }
}

void publish_state_if_changed(esphome::text_sensor::TextSensor *entity, const char *state) {
  if (entity->get_raw_state() != state) {
    entity->publish_state(state);
  }
}

#define MINUET_LOGD_IF_CHANGED(size, fmt, ...) \
  if (1) { \
    static char last_output_buf[(size)]{}; \
    char output_buf[(size)]; \
    esphome::buf_append_printf(output_buf, (size), 0, (fmt), ##__VA_ARGS__); \
    if (strcmp(last_output_buf, output_buf) != 0) { \
      ESP_LOGD(minuet::TAG, "%s", output_buf); \
      strcpy(last_output_buf, output_buf); \
    } \
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
