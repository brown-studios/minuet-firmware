// MINUET AUTO MODE GOVERNOR HEADER
//
// The auto mode governor manages the lifecycle and interactions of the all of the auto mode
// controllers, each of which implements a particular automatic control algorithm.  Once a
// determination has been made for the appropriate state of the fan and lid, the governor signals
// the coordinator to apply it.

#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "controllers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sensors.h"
#include "utility.h"

namespace minuet {
namespace auto_mode {

bool should_lid_open(int fan_speed) {
  switch (get_auto_lid_position()) {
    default:
    case AutoLidPosition::AUTO:
      return fan_speed != 0;
    case AutoLidPosition::OPEN:
      return true;
    case AutoLidPosition::CLOSED:
      return false;
  }
}

class Governor {
 public:
  // Called by the coordinator to start the automatic control loop.
  // The governor invokes the provided callback to when the next output state is available.
  using CoordinatorCallback = void (*)();
  void start(CoordinatorCallback callback);

  // Called by the coordinator to stop the automatic control loop.
  void stop();

  // Called by the coordinator while the automatic control loop is running to get the requested output state.
  Output get_output() const {
    return this->output_;
  }

  void update_output_and_notify_coordinator();

 private:
  bool update_output_();

  const esphome::FixedVector<Controller *> controllers_{make_controllers()};

  CoordinatorCallback coordinator_callback_{};
  Output output_{};
};

void Governor::start(CoordinatorCallback callback) {
  ESP_LOGD(minuet::TAG, "Auto mode governor started");
  assert(callback);
  assert(!this->coordinator_callback_);

  this->coordinator_callback_ = callback;
  this->update_output_();
}

void Governor::stop() {
  ESP_LOGD(minuet::TAG, "Auto mode governor stopped");
  assert(this->coordinator_callback_);

  this->coordinator_callback_ = nullptr;
  this->update_output_();
}

void Governor::update_output_and_notify_coordinator() {
  if (this->update_output_() && this->coordinator_callback_) {
    this->coordinator_callback_();
  }
}

bool Governor::update_output_() {
  const uint64_t now_ms = millis_64();
  const bool run = !!this->coordinator_callback_;

  Demand combined_demand{};
  for (auto controller : this->controllers_) {
    combined_demand.add(controller->prepare(now_ms, run));
  }

  Output output{};
  if (run && !combined_demand.inhibit_ventilation) {
    output.fan_speed = combined_demand.minimum_fan_speed.value_or(0);
    output.lid_open = should_lid_open(output.fan_speed);
  }
  bool output_changed = false;
  if (this->output_ != output) {
    this->output_ = output;
    output_changed = true;
  }

  for (auto controller : this->controllers_) {
    controller->apply(this->output_);
  }
  return output_changed;
}

Governor governor;

void schedule_governor_update() {
  esphome::App.scheduler.set_timeout(
      nullptr, "governor update", 0, [] { governor.update_output_and_notify_coordinator(); });
}

}  // namespace auto_mode
}  // namespace minuet
