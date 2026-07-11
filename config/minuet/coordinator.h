// MINUET COORDINATOR HEADER
//
// The coordinator couples the fan, lid, and auto mode entities to ensure consistent behavior
// under manual or automatic control.
//
// Unfortunately, the coordinator does not exclusively own these entities; other components can
// perform actions on the entities and the coordinator may need to perform additional side-effects
// in response to ensure consistent behavior, such as overriding automatic control and returning
// to manual mode.  Thus the coordinator's primary role is to encapsulate these subtle interactions
// and to keep the resulting complexity out of other subsystems.
//
// The coordinator receives an event each time the fan, lid, and auto mode entities change state.
// Before applying additional side-effects, the coordinator determines whether the event was
// caused internally by the coordinator's own actions or externally by other components and it
// handles the event accordingly.  The coordinator flags its actions as `internal` to emulate atomic
// transactions across multiple entities and it prevents reentrance by suppressing unnecessary
// side-effects.
//
// The coordinator also persists the state of the fan and lid to restore at power up when the
// power on restore function is enabled.
//
// The `command_` prefix designates functions called by external components on behalf of the user.
//
// The `on_` prefix designates events that are called by entities in response to state changes.

#pragma once

#include "auto_mode.h"
#include "esphome/core/preferences.h"
#include "fan.h"
#include "lid.h"
#include "safety_lock.h"
#include "utility.h"

#include <optional>

namespace minuet {
namespace coordinator {

constexpr int DEFAULT_FAN_SPEED = 3;

// Random version number, change as the restore state struct layout or interpretation evolves.
constexpr uint32_t RESTORE_STATE_VERSION = 0xE69A41A1;
struct RestoreState {
  bool fan_on;
  int fan_speed;
  bool fan_exhaust;
  bool lid_open;
};

struct Call {
  std::optional<bool> fan_on;
  std::optional<int> fan_speed;
  std::optional<bool> fan_exhaust;
  std::optional<bool> lid_open;
  std::optional<esphome::climate::ClimateMode> auto_mode;
  std::optional<float> auto_temperature;
  const char *safety_lock_description;
};

enum class ModeChange : uint8_t {
  MANUAL = 0,
  MANUAL_OVERRIDE = 1,
  AUTO = 2,
};

bool was_last_mode_change_manual_override() {
  return minuet_mode_change->get_last_event_type_index() == uint8_t(ModeChange::MANUAL_OVERRIDE);
}

void schedule_reconcile();

class Coordinator {
 public:
  void on_boot();
  void on_fan_state(bool fan_on, int fan_speed, bool fan_exhaust);
  void on_lid_action();
  void on_auto_state(esphome::climate::ClimateMode auto_mode);
  void on_safety_lock_update();

  void clear_lid_position();
  void confirm_lid_position();

  void reconcile();
  void set_manual_override(const char *reason);
  void apply_call(Call call);

 private:
  enum class Event : uint8_t {
    NONE = 0,
    FAN_STATE,
    LID_ACTION,
    AUTO_STATE,
  };

  void update_safety_lock_ok_(const char *description, minuet::safety_lock::Stage stage);

  void report_mode_change_(ModeChange change);

  template<typename T> void expect_internal_event_(Event event, T func);
  void reject_internal_event_reentrance_();
  bool finish_event_(Event event);

  void save_state_();
  RestoreState load_state_();

  bool booted_{};
  int manual_fan_speed_{};
  bool auto_running_{};
  bool safety_lock_ok_{};

  // Assume the lid position is accurate except during major state changes to minimize
  // noise from the lid mechanism.  The assumed position may become inaccurate when the
  // user turns the knob manually to open or close the lid or a mechanical fault occurs.
  bool assume_lid_position_{};

  Event internal_event_{};

  bool prior_fan_on_{};
  int prior_fan_speed_{};
  esphome::climate::ClimateMode prior_auto_mode_{};

  esphome::ESPPreferenceObject rtc_;
};

Coordinator coordinator{};

void Coordinator::on_boot() {
  ESP_LOGI(minuet::TAG, "Initializing the coordinator");

  this->rtc_ = esphome::global_preferences->make_preference<RestoreState>(RESTORE_STATE_VERSION);
  RestoreState state = this->load_state_();
  this->manual_fan_speed_ = state.fan_speed;
  this->booted_ = true;

  minuet::auto_mode::init();

  minuet::safety_lock::on_update.add([this]() { this->on_safety_lock_update(); });
  this->update_safety_lock_ok_("check whether safety locks are active", minuet::safety_lock::Stage::SPECULATIVE);

  Call call{
      .fan_on = this->prior_fan_on_ = state.fan_on,
      .fan_speed = this->prior_fan_speed_ = state.fan_speed,
      .fan_exhaust = state.fan_exhaust,
      .lid_open = state.lid_open,
  };
  this->prior_auto_mode_ = minuet::auto_mode::get_auto_mode();
  this->apply_call(call);
}

void Coordinator::on_fan_state(bool fan_on, int fan_speed, bool fan_exhaust) {
  if (!this->booted_)
    return;
  const bool fan_on_changed = this->prior_fan_on_ != fan_on;
  const bool fan_speed_changed = this->prior_fan_speed_ != fan_speed;
  this->prior_fan_on_ = fan_on;
  this->prior_fan_speed_ = fan_speed;
  if (this->finish_event_(Event::FAN_STATE)) {
    if (fan_on_changed || fan_speed_changed) {
      this->set_manual_override("fan state change event");
    }
    this->save_state_();
  }
}

void Coordinator::on_lid_action() {
  if (!this->booted_)
    return;
  this->clear_lid_position();
  if (this->finish_event_(Event::LID_ACTION)) {
    this->set_manual_override("lid action");
    this->save_state_();
  }
}

void Coordinator::on_auto_state(esphome::climate::ClimateMode auto_mode) {
  if (!this->booted_)
    return;
  const bool prior_auto_on = this->prior_auto_mode_ != esphome::climate::ClimateMode::CLIMATE_MODE_OFF;
  const bool auto_on = auto_mode != esphome::climate::ClimateMode::CLIMATE_MODE_OFF;
  const bool auto_on_changed = prior_auto_on != auto_on;
  this->prior_auto_mode_ = auto_mode;
  if (this->finish_event_(Event::AUTO_STATE)) {
    if (auto_on_changed) {
      this->report_mode_change_(auto_on ? ModeChange::AUTO : ModeChange::MANUAL);
      this->reconcile();
    }
  }
}

void Coordinator::on_safety_lock_update() {
  const bool was_ok = this->safety_lock_ok_;
  this->update_safety_lock_ok_("check whether to keep fan on or lid open",
      minuet_fan->state || minuet::lid::is_open_or_opening() ? minuet::safety_lock::Stage::ONGOING
                                                             : minuet::safety_lock::Stage::SPECULATIVE);
  if (this->safety_lock_ok_ != was_ok) {
    ESP_LOGI(minuet::TAG, "Safety lock: %s", this->safety_lock_ok_ ? "ok" : "inhibited");
    if (!this->safety_lock_ok_) {
      this->clear_lid_position();
    }
    this->reconcile();
  }
}

void Coordinator::clear_lid_position() {
  if (this->assume_lid_position_) {
    ESP_LOGI(minuet::TAG, "Invalidated assumed lid position");
    this->assume_lid_position_ = false;
  }
}

void Coordinator::confirm_lid_position() {
  ESP_LOGI(minuet::TAG, "Confirmed lid position: %s", minuet::lid::is_open_or_opening() ? "open" : "closed");
  this->assume_lid_position_ = true;
}

void Coordinator::reconcile() {
  this->apply_call({});
}

void Coordinator::set_manual_override(const char *reason) {
  if (minuet::auto_mode::is_active()) {
    ESP_LOGI(minuet::TAG, "Manual override caused by %s", reason);
    Call call{
        .fan_on = minuet_fan->state,
        .fan_speed = minuet_fan->speed,
        .lid_open = minuet::lid::is_open_or_opening(),
        .auto_mode = esphome::climate::CLIMATE_MODE_OFF,
    };
    this->apply_call(call);
    this->report_mode_change_(ModeChange::MANUAL_OVERRIDE);
  }
}

void Coordinator::apply_call(Call call) {
  if (!this->booted_) {
    ESP_LOGW(minuet::TAG, "Coordinator not ready");
    return;
  }

  // Apply auto mode first because its output might change the fan and lid state.
  if (call.auto_mode.has_value() || call.auto_temperature.has_value()) {
    const auto new_mode = call.auto_mode.value_or(minuet::auto_mode::get_auto_mode());
    const float new_temperature = call.auto_temperature.value_or(minuet_auto_thermostat->target_temperature);
    if (minuet::auto_mode::get_auto_mode() != new_mode ||
        minuet_auto_thermostat->target_temperature != new_temperature) {
      this->expect_internal_event_(Event::AUTO_STATE, [=] {
        minuet_auto_thermostat->make_call().set_mode(new_mode).set_target_temperature(new_temperature).perform();
      });
    }
  }

  // Save the fan speed for later.
  if (call.fan_speed.has_value()) {
    this->manual_fan_speed_ = call.fan_speed.value();
  }

  // SAFETY LOCK CHECK: Alert when a manual call to turn the fan on or open the lid is inhibited.
  if (call.safety_lock_description && !minuet::auto_mode::is_active() &&
      (call.fan_on.value_or(false) || call.lid_open.value_or(false))) {
    this->update_safety_lock_ok_(call.safety_lock_description, minuet::safety_lock::Stage::INITIATING);
  }

  // Ensure that the auto mode governor is in a consistent state and update the call.
  if (minuet::auto_mode::is_active() && this->safety_lock_ok_) {
    if (!this->auto_running_) {
      if (!call.fan_exhaust.has_value()) {
        switch (minuet::auto_mode::get_auto_fan_direction()) {
          case minuet::auto_mode::AutoFanDirection::AIR_OUT:
            call.fan_exhaust = true;
            break;
          case minuet::auto_mode::AutoFanDirection::AIR_IN:
            call.fan_exhaust = false;
            break;
          default:
            break;
        }
      }
      minuet::auto_mode::governor.start(schedule_reconcile);
      this->auto_running_ = true;
    }
    const auto auto_output = minuet::auto_mode::governor.get_output();
    call.fan_on = auto_output.fan_speed > 0;
    call.fan_speed = auto_output.fan_speed > 0 ? auto_output.fan_speed : this->manual_fan_speed_;
    call.lid_open = auto_output.lid_open;
  } else {
    if (this->auto_running_) {
      if (!call.fan_on.has_value()) {
        call.fan_on = false;
      }
      if (!call.fan_speed.has_value()) {
        call.fan_speed = this->manual_fan_speed_;
      }
      if (!call.lid_open.has_value()) {
        call.lid_open = false;
      }
      minuet::auto_mode::governor.stop();
      this->auto_running_ = false;
    }
    // SAFETY LOCK CHECK: Stop the fan and close the lid now.
    if (!this->safety_lock_ok_) {
      call.fan_on = false;
      call.lid_open = false;
    }
  }

  // Apply the fan state.
  if (call.fan_on.has_value() || call.fan_speed.has_value() || call.fan_exhaust.has_value()) {
    const bool new_state = call.fan_on.value_or(minuet_fan->state);
    const int new_speed = call.fan_speed.value_or(minuet_fan->speed);
    const esphome::fan::FanDirection new_direction =
        call.fan_exhaust.has_value() ? minuet::fan::make_direction(call.fan_exhaust.value()) : minuet_fan->direction;
    if (minuet_fan->state != new_state || minuet_fan->speed != new_speed || minuet_fan->direction != new_direction) {
      this->expect_internal_event_(Event::FAN_STATE, [=] {
        minuet_fan->make_call().set_state(new_state).set_speed(new_speed).set_direction(new_direction).perform();
      });
    }
  }

  // Apply the lid state.
  if (call.lid_open.has_value()) {
    const bool new_open = call.lid_open.value();
    if (new_open && (!this->assume_lid_position_ || !minuet::lid::is_open_or_opening())) {
      this->expect_internal_event_(Event::LID_ACTION, [=] { minuet_lid->make_call().set_command_open().perform(); });
    }
    if (!new_open && (!this->assume_lid_position_ || minuet::lid::is_open_or_opening())) {
      this->expect_internal_event_(Event::LID_ACTION, [=] { minuet_lid->make_call().set_command_close().perform(); });
    }
  }

  // Persist the state.
  this->save_state_();
}

void Coordinator::update_safety_lock_ok_(const char *description, minuet::safety_lock::Stage stage) {
  this->safety_lock_ok_ =
      minuet::safety_lock::validate_operation({.description = description, .stage = stage, .rain_sensitive = true});
}

void Coordinator::report_mode_change_(ModeChange change) {
  minuet_mode_change->trigger(std::string(minuet_mode_change->get_event_type(uint8_t(change))));
}

template<typename T> void Coordinator::expect_internal_event_(Event event, T func) {
  this->reject_internal_event_reentrance_();
  this->internal_event_ = event;
  func();
  this->internal_event_ = Event::NONE;
}

void Coordinator::reject_internal_event_reentrance_() {
  if (this->internal_event_ != Event::NONE) {
    ESP_LOGW(minuet::TAG, "Coordinator detected reentrance during event %u", unsigned(this->internal_event_));
  }
}

bool Coordinator::finish_event_(Event event) {
  if (this->internal_event_ == Event::NONE) {
    return true;  // external event cause
  } else if (this->internal_event_ != event) {
    ESP_LOGW(
        minuet::TAG, "Coordinator received event %u but expected %u", unsigned(event), unsigned(this->internal_event_));
  }
  return false;  // internal event cause
}

void Coordinator::save_state_() {
  const bool auto_mode_is_active = minuet::auto_mode::is_active();
  RestoreState state{
      .fan_on = auto_mode_is_active ? false : minuet_fan->state,
      .fan_speed = this->manual_fan_speed_,
      .fan_exhaust = minuet::fan::direction_is_exhaust(minuet_fan->direction),
      .lid_open = auto_mode_is_active ? false : minuet::lid::is_open_or_opening(),
  };
  this->rtc_.save(&state);
}

RestoreState Coordinator::load_state_() {
  RestoreState state{
      .fan_on = false,
      .fan_speed = DEFAULT_FAN_SPEED,
      .fan_exhaust = false,
      .lid_open = false,
  };
  if (minuet_power_on_behavior->active_index() == 1) {
    this->rtc_.load(&state);
  }
  return state;
}

void command_fan_direction(bool exhaust) {
  // This command can be performed in both auto and manual mode (without a manual override).
  minuet_fan->make_call().set_direction(minuet::fan::make_direction(exhaust)).perform();
}

void command_manual_override() {
  coordinator.set_manual_override("manual override command");
}

void command_manual_power_on() {
  coordinator.set_manual_override("power on command");
  Call call{
      .fan_on = true,
      .lid_open = true,
      .safety_lock_description = "power on",
  };
  coordinator.apply_call(call);
}

void command_manual_power_off() {
  coordinator.set_manual_override("power off command");
  Call call{
      .fan_on = false,
      .lid_open = false,
  };
  coordinator.apply_call(call);
}

void command_manual_fan_speed(int speed) {
  // Performing a call on the entity has the side-effect of a manual override.
  minuet_fan->make_call().set_speed(speed).perform();
}

void command_manual_lid_open() {
  // Performing a call on the entity has the side-effect of a manual override.
  minuet_lid->make_call().set_command_open().perform();
}

void command_manual_lid_close() {
  // Performing a call on the entity has the side-effect of a manual override.
  minuet_lid->make_call().set_command_close().perform();
}

void command_manual_lid_toggle() {
  // Performing a call on the entity has the side-effect of a manual override.
  minuet_lid->make_call().set_command_toggle().perform();
}

void command_auto_on() {
  minuet_auto_thermostat->make_call().set_mode(esphome::climate::ClimateMode::CLIMATE_MODE_FAN_ONLY).perform();
}

void command_auto_off() {
  minuet_auto_thermostat->make_call().set_mode(esphome::climate::ClimateMode::CLIMATE_MODE_OFF).perform();
}

void command_auto_fan_direction(minuet::auto_mode::AutoFanDirection fan_direction) {
  minuet::auto_mode::set_auto_fan_direction(fan_direction);
}

void command_auto_lid_position(minuet::auto_mode::AutoLidPosition lid_position) {
  minuet::auto_mode::set_auto_lid_position(lid_position);
}

void command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode fan_mode) {
  auto call = minuet_auto_thermostat->make_call();
  minuet::auto_mode::set_auto_thermostat_call_fan_mode(call, fan_mode).perform();
}

void command_auto_thermostat_temperature_change(int steps) {
  minuet_auto_thermostat->make_call()
      .set_target_temperature(minuet_auto_thermostat->target_temperature +
          steps * minuet::auto_mode::AUTO_THERMOSTAT_TARGET_TEMPERATURE_STEP)
      .perform();
}

void command_auto_reset() {
  auto call = minuet_auto_thermostat->make_call()
                  .set_mode(esphome::climate::ClimateMode::CLIMATE_MODE_OFF)
                  .set_target_temperature(minuet::auto_mode::AUTO_THERMOSTAT_TARGET_TEMPERATURE_DEFAULT);
  minuet::auto_mode::set_auto_thermostat_call_fan_mode(call, minuet::auto_mode::AutoFanMode::AUTO).perform();
  command_auto_fan_direction(minuet::auto_mode::AutoFanDirection::AUTO);
  command_auto_lid_position(minuet::auto_mode::AutoLidPosition::AUTO);
}

void command_ir_remote_state(
    bool fan_on, int fan_speed, bool fan_exhaust, bool lid_open, bool auto_mode, float auto_temperature) {
  Call call{
      .fan_on = fan_on,
      .fan_speed = fan_speed,
      .fan_exhaust = fan_exhaust,
      .lid_open = lid_open,
      .auto_mode = auto_mode ? esphome::climate::ClimateMode::CLIMATE_MODE_FAN_ONLY
                             : esphome::climate::ClimateMode::CLIMATE_MODE_OFF,
      .auto_temperature = auto_temperature,
      .safety_lock_description = "IR remote command",
  };
  coordinator.apply_call(call);
}

void schedule_reconcile() {
  esphome::App.scheduler.set_timeout(nullptr, "coordinator reconcile", 0, [] { coordinator.reconcile(); });
}

}  // namespace coordinator
}  // namespace minuet
