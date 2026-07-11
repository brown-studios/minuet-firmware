// MINUET AUTO MODE HUMIDISTAT HEADER

#pragma once

#include "common.h"
#include "sensors.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

bool get_humidistat_default_on() {
  return (MINUET_AUTO_HUMIDISTAT_DEFAULT_ON);
}

class Humidistat final : public Controller {
 public:
  Demand prepare(uint64_t now_ms, bool run) override;
  void apply(const Output &output) override;

 private:
  enum class Status : uint8_t {
    OFF = 0,
    IDLE,
    TIMEOUT,
    DEMAND,
  };
  static constexpr const char *STATUS_STRINGS[] = {"OFF", "IDLE", "TIMEOUT", "DEMAND"};

  Status status_{};
  Timer runtime_timer_{};
  float current_temperature_{NAN};
  float current_humidity_{NAN};
};

Demand Humidistat::prepare(uint64_t now_ms, bool run) {
  const bool on = minuet_auto_humidistat->mode == esphome::climate::CLIMATE_MODE_AUTO;
  const int fan_speed = int(minuet_auto_humidistat_fan_speed->state);
  const float trigger_threshold = minuet_auto_humidistat_trigger_threshold->state;
  const float clear_threshold = minuet_auto_humidistat_clear_threshold->state;
  const float minimum_temperature = minuet_auto_humidistat_minimum_temperature->state;
  const float maximum_runtime_ms = minuet_auto_humidistat_maximum_runtime->state * 60 * 1000;
  this->current_temperature_ = sensors.temperature.get_state();
  this->current_humidity_ = sensors.humidity.get_state();

  if (!run || !on || std::isnan(this->current_temperature_) || std::isnan(this->current_humidity_)) {
    this->status_ = Status::OFF;
    this->runtime_timer_.cancel();
    return {};
  }

  if (this->current_temperature_ < minimum_temperature || this->current_humidity_ <= clear_threshold) {
    this->status_ = Status::IDLE;
  } else if (this->current_humidity_ >= trigger_threshold) {
    if (this->status_ != Status::DEMAND && this->status_ != Status::TIMEOUT) {
      this->status_ = Status::DEMAND;
      this->runtime_timer_.reset(now_ms);
    }
  } else if (this->status_ == Status::OFF) {
    this->status_ = Status::IDLE;
  }
  if (this->status_ != Status::DEMAND) {
    this->runtime_timer_.cancel();
    return {};
  }
  if (this->runtime_timer_.update(now_ms, maximum_runtime_ms)) {
    this->status_ = Status::TIMEOUT;
    return {};
  }
  return {.minimum_fan_speed = fan_speed};
}

void Humidistat::apply(const Output &output) {
  MINUET_LOGD_IF_CHANGED(100, "Auto humidistat: status %s, %.1f °C, %.1f %%RH", STATUS_STRINGS[size_t(this->status_)],
      this->current_temperature_, this->current_humidity_);

  const auto action = this->status_ == Status::OFF            ? esphome::climate::CLIMATE_ACTION_OFF
      : this->status_ >= Status::DEMAND && output.is_active() ? esphome::climate::CLIMATE_ACTION_FAN
                                                              : esphome::climate::CLIMATE_ACTION_IDLE;
  if (!equal_or_both_nan(minuet_auto_humidistat->current_temperature, this->current_temperature_) ||
      !equal_or_both_nan(minuet_auto_humidistat->current_humidity, this->current_humidity_) ||
      minuet_auto_humidistat->action != action) {
    minuet_auto_humidistat->current_temperature = this->current_temperature_;
    minuet_auto_humidistat->current_humidity = this->current_humidity_;
    minuet_auto_humidistat->action = action;
    minuet_auto_humidistat->publish_state();
  }
}

}  // namespace auto_mode
}  // namespace minuet
