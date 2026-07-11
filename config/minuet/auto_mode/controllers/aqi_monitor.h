// MINUET AUTO MODE AQI MONITOR HEADER

#pragma once

#include "common.h"
#include "sensors.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

bool get_aqi_monitor_default_on() {
  return (MINUET_AUTO_AQI_MONITOR_DEFAULT_ON);
}

class AQIMonitor final : public Controller {
 public:
  Demand prepare(uint64_t now_ms, bool run) override;
  void apply(const Output &output) override;

 private:
  enum class Status : uint8_t {
    OFF = 0,
    IDLE,
    IDLE_TO_INHIBIT,
    INHIBIT_TO_IDLE,
    INHIBIT,
  };
  static constexpr const char *STATUS_STRINGS[] = {"OFF", "IDLE", "IDLE_TO_INHIBIT", "INHIBIT_TO_IDLE", "INHIBIT"};

  Status status_{};
  float current_aqi_{NAN};
  Timer clear_timer_{};
  Timer trigger_timer_{};
};

Demand AQIMonitor::prepare(uint64_t now_ms, bool run) {
  const bool on = minuet_auto_aqi_monitor->mode == esphome::climate::CLIMATE_MODE_AUTO;
  const float clear_threshold = minuet_auto_aqi_monitor_clear_threshold->state;
  const float trigger_threshold = std::max(minuet_auto_aqi_monitor_trigger_threshold->state, clear_threshold);
  const float clear_duration_ms = minuet_auto_aqi_monitor_clear_duration->state * 60 * 1000;
  const float trigger_duration_ms = minuet_auto_aqi_monitor_trigger_duration->state * 60 * 1000;
  this->current_aqi_ = sensors.aqi.get_state();

  if (!run || !on || std::isnan(this->current_aqi_)) {
    this->status_ = Status::OFF;
    this->clear_timer_.cancel();
    this->trigger_timer_.cancel();
    return {};
  }

  if (this->status_ == Status::OFF) {
    this->status_ = Status::IDLE;
  }
  if (this->status_ == Status::IDLE && this->current_aqi_ >= trigger_threshold) {
    this->status_ = Status::IDLE_TO_INHIBIT;
    this->trigger_timer_.reset(now_ms);
  }
  if (this->status_ == Status::IDLE_TO_INHIBIT) {
    if (this->current_aqi_ >= trigger_threshold) {
      if (this->trigger_timer_.update(now_ms, trigger_duration_ms)) {
        this->status_ = Status::INHIBIT;
      }
    } else {
      this->status_ = Status::IDLE;
      this->trigger_timer_.cancel();
    }
  }
  if (this->status_ == Status::INHIBIT && this->current_aqi_ <= clear_threshold) {
    this->status_ = Status::INHIBIT_TO_IDLE;
    this->clear_timer_.reset(now_ms);
  }
  if (this->status_ == Status::INHIBIT_TO_IDLE) {
    if (this->current_aqi_ <= clear_threshold) {
      if (this->clear_timer_.update(now_ms, clear_duration_ms)) {
        this->status_ = Status::IDLE;
      }
    } else {
      this->status_ = Status::INHIBIT;
      this->clear_timer_.cancel();
    }
  }
  return {.inhibit_ventilation = this->status_ >= Status::INHIBIT_TO_IDLE};
}

void AQIMonitor::apply(const Output &output) {
  MINUET_LOGD_IF_CHANGED(
      100, "Auto AQI monitor: status %s, %0.f aqi", STATUS_STRINGS[size_t(this->status_)], this->current_aqi_);

  // There isn't really a suitable action to express that the climate entity is inhibiting operation.
  // CLIMATE_ACTION_FAN could be confusing so we arbitrarily use CLIMATE_ACTION_DRYING.
  const auto action = this->status_ == Status::OFF                      ? esphome::climate::CLIMATE_ACTION_OFF
      : this->status_ >= Status::INHIBIT_TO_IDLE && !output.is_active() ? esphome::climate::CLIMATE_ACTION_DRYING
                                                                        : esphome::climate::CLIMATE_ACTION_IDLE;
  if (minuet_auto_aqi_monitor->action != action) {
    minuet_auto_aqi_monitor->action = action;
    minuet_auto_aqi_monitor->publish_state();
  }
  if (!equal_or_both_nan(minuet_auto_aqi_monitor_current_aqi->state, this->current_aqi_)) {
    minuet_auto_aqi_monitor_current_aqi->publish_state(this->current_aqi_);
  }
}

}  // namespace auto_mode
}  // namespace minuet
