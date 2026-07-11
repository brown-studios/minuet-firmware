// MINUET AUTO MODE CO2 MONITOR HEADER

#pragma once

#include "common.h"
#include "sensors.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

bool get_co2_monitor_default_on() {
  return (MINUET_AUTO_CO2_MONITOR_DEFAULT_ON);
}

class CO2Monitor final : public Controller {
 public:
  Demand prepare(uint64_t now_ms, bool run) override;
  void apply(const Output &output) override;

 private:
  enum class Status : uint8_t {
    OFF = 0,
    IDLE,
    DEMAND_LEVEL_1,
    DEMAND_LEVEL_2,
  };
  static constexpr const char *STATUS_STRINGS[] = {"OFF", "IDLE", "DEMAND_LEVEL_1", "DEMAND_LEVEL_2"};

  Status status_{};
  float current_co2_{NAN};
};

Demand CO2Monitor::prepare(uint64_t now_ms, bool run) {
  const bool on = minuet_auto_co2_monitor->mode == esphome::climate::CLIMATE_MODE_AUTO;
  const float clear_threshold = minuet_auto_co2_monitor_clear_threshold->state;
  const float level_1_threshold = std::max(minuet_auto_co2_monitor_level_1_threshold->state, clear_threshold);
  const float level_2_threshold = std::max(minuet_auto_co2_monitor_level_2_threshold->state, level_1_threshold);
  const float midpoint_threshold = (level_1_threshold + level_2_threshold) / 2;
  const int level_1_fan_speed = int(minuet_auto_co2_monitor_level_1_fan_speed->state);
  const int level_2_fan_speed = std::max(int(minuet_auto_co2_monitor_level_2_fan_speed->state), level_1_fan_speed);
  this->current_co2_ = sensors.co2.get_state();

  if (!run || !on || std::isnan(this->current_co2_)) {
    this->status_ = Status::OFF;
    return {};
  }

  if (this->current_co2_ <= clear_threshold) {
    this->status_ = Status::IDLE;
  } else if (this->current_co2_ >= level_2_threshold) {
    this->status_ = Status::DEMAND_LEVEL_2;
  } else if (this->current_co2_ >= level_1_threshold &&
      (this->status_ != Status::DEMAND_LEVEL_2 || this->current_co2_ <= midpoint_threshold)) {
    this->status_ = Status::DEMAND_LEVEL_1;
  }
  if (this->status_ == Status::OFF) {
    this->status_ = Status::IDLE;
  }

  switch (this->status_) {
    default:
    case Status::IDLE:
      return {};
    case Status::DEMAND_LEVEL_1:
      return {.minimum_fan_speed = level_1_fan_speed};
    case Status::DEMAND_LEVEL_2:
      return {.minimum_fan_speed = level_2_fan_speed};
  }
}

void CO2Monitor::apply(const Output &output) {
  MINUET_LOGD_IF_CHANGED(
      100, "Auto CO₂ monitor: status %s, %0.f ppm", STATUS_STRINGS[size_t(this->status_)], this->current_co2_);

  const auto action = this->status_ == Status::OFF                    ? esphome::climate::CLIMATE_ACTION_OFF
      : this->status_ >= Status::DEMAND_LEVEL_1 && output.is_active() ? esphome::climate::CLIMATE_ACTION_FAN
                                                                      : esphome::climate::CLIMATE_ACTION_IDLE;
  if (minuet_auto_co2_monitor->action != action) {
    minuet_auto_co2_monitor->action = action;
    minuet_auto_co2_monitor->publish_state();
  }
  if (!equal_or_both_nan(minuet_auto_co2_monitor_current_co2->state, this->current_co2_)) {
    minuet_auto_co2_monitor_current_co2->publish_state(this->current_co2_);
  }
}

}  // namespace auto_mode
}  // namespace minuet
