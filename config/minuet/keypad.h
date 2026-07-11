// MINUET KEYPAD HEADER
//
// Reads the keypad keys, dispatches actions, and updates indicators.

#pragma once

#include <algorithm>
#include <cstdint>

#include "accessory.h"
#include "auto_mode.h"
#include "coordinator.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/cover/cover.h"
#include "fan.h"
#ifdef MINUET_RADIO
#include "radio.h"
#endif
#include "tone.h"
#include "utility.h"

namespace minuet {
namespace keypad {

bool has_enhanced_controls() {
  return minuet_keypad_controls->active_index().value_or(0) == 0;
}

bool can_indicators_sleep() {
  return minuet_keypad_indicators->active_index().value_or(0) == 1;
}

// The keypad controls behave differently based on the control scheme and current mode
// so each state is represented as a menu of possible actions.
enum class Menu {
  STANDARD_MANUAL,
  STANDARD_AUTO,
  ENHANCED,
};

static Menu current_menu() {
  if (has_enhanced_controls())
    return Menu::ENHANCED;
  if (minuet::auto_mode::is_active())
    return Menu::STANDARD_AUTO;
  return Menu::STANDARD_MANUAL;
}

struct IndicatorPattern {
  uint8_t period;
  uint8_t loops;
};

static constexpr IndicatorPattern INDICATOR_PATTERN_OFF = {};
static constexpr IndicatorPattern INDICATOR_PATTERN_ON = {.period = 1};
static constexpr IndicatorPattern INDICATOR_PATTERN_ATTENTION = {.period = 2};
static constexpr IndicatorPattern INDICATOR_PATTERN_BLINK = {.period = 4};
static constexpr IndicatorPattern INDICATOR_PATTERN_BLINK_BRIEFLY = {.period = 4, .loops = 10};
static constexpr IndicatorPattern INDICATOR_PATTERN_CONFIRM_2 = {.period = 8, .loops = 2};
static constexpr IndicatorPattern INDICATOR_PATTERN_CONFIRM_3 = {.period = 8, .loops = 3};

class Indicator {
 public:
  static constexpr uint8_t CYCLES_UNTIL_SLEEP = 8000 / MINUET_KEYPAD_INDICATOR_POLL_INTERVAL_MS;
  static constexpr uint8_t NUM_PRIORITIES = 4;

  void start_pattern(IndicatorPattern pattern, uint8_t priority) {
    if (priority < NUM_PRIORITIES) {
      this->states_[priority] = {.pattern = pattern};
      if (pattern.period) {
        this->wake();
      }
    }
  }

  void set_condition(uint8_t condition, const IndicatorPattern patterns[], uint8_t priority) {
    if (priority < NUM_PRIORITIES && this->conditions_[priority] != condition) {
      this->conditions_[priority] = condition;
      this->start_pattern(patterns[condition], priority);
    }
  }

  void wake() {
    this->cycles_until_sleep_ = CYCLES_UNTIL_SLEEP;
  }

  void sleep() {
    this->cycles_until_sleep_ = 0;
  }

  bool poll(uint8_t cycle, bool can_sleep) {
    if (this->cycles_until_sleep_)
      this->cycles_until_sleep_ -= 1;

    for (uint8_t i = 0; i < NUM_PRIORITIES; i++) {
      auto &state = this->states_[i];
      if (!state.pattern.period)
        continue;  // pattern is always off
      if (state.pattern.loops && state.iteration > state.pattern.loops)
        continue;  // done with this pattern
      const uint8_t phase = cycle % state.pattern.period;
      if (!state.iteration) {
        if (phase)
          return false;  // wait for the phase to align before starting the pattern
        state.iteration = 1;
      } else if (!phase) {
        state.iteration += 1;
        if (state.pattern.loops && state.iteration > state.pattern.loops)
          continue;  // done with this pattern
      }
      if (can_sleep && !phase && !this->cycles_until_sleep_ && !state.pattern.loops)
        return false;  // sleep (don't sleep mid-phase or during counted patterns)
      return phase >= state.pattern.period / 2;
    }
    return false;
  }

 private:
  struct State {
    IndicatorPattern pattern;
    uint8_t iteration;
  };
  State states_[NUM_PRIORITIES]{};
  uint8_t conditions_[NUM_PRIORITIES]{};
  uint8_t cycles_until_sleep_{};
};

// Scans the keypad matrix and dispatches actions.
class Keypad {
 public:
  void poll_keys();
  void poll_indicators();
  void wake_indicators();
  void sleep_indicators();

 private:
  // One bit for each key decoded from the matrix.
  static constexpr uint32_t KEY_UP = 1u << 0;
  static constexpr uint32_t KEY_DOWN = 1u << 1;
  static constexpr uint32_t KEY_RAIN = 1u << 2;
  static constexpr uint32_t KEY_POWER = 1u << 3;
  static constexpr uint32_t KEY_DIRECTION = 1u << 4;
  static constexpr uint32_t KEY_AUTO = 1u << 5;
  static constexpr uint32_t KEY_4KEY_CLOSE = 1u << 6;
  static constexpr uint32_t KEY_4KEY_OPEN = 1u << 7;
  static constexpr uint32_t KEY_4KEY_OFF = 1u << 8;
  static constexpr uint32_t KEY_4KEY_ON = 1u << 9;

  // Valid combinations of keys that can be pressed together.
  // Typically one of the keys acts as a modifier that is held while some other key is pressed.
  static constexpr uint32_t KEY_COMBO_OPEN_CLOSE = KEY_UP | KEY_DOWN;
  static constexpr uint32_t KEY_COMBO_AUTO_UP = KEY_AUTO | KEY_UP;
  static constexpr uint32_t KEY_COMBO_AUTO_DOWN = KEY_AUTO | KEY_DOWN;
  static constexpr uint32_t KEY_COMBO_AUTO_DIRECTION = KEY_AUTO | KEY_DIRECTION;
  static constexpr uint32_t KEY_COMBO_AUTO_POWER = KEY_AUTO | KEY_POWER;
  static constexpr uint32_t KEY_COMBO_AUTO_OPEN_CLOSE = KEY_AUTO | KEY_COMBO_OPEN_CLOSE;
  static constexpr uint32_t KEY_COMBO_POWER_UP = KEY_POWER | KEY_UP;
  static constexpr uint32_t KEY_COMBO_POWER_DOWN = KEY_POWER | KEY_DOWN;
  static constexpr uint32_t KEY_COMBO_POWER_DIRECTION = KEY_POWER | KEY_DIRECTION;
  static constexpr uint32_t KEY_COMBO_POWER_OPEN_CLOSE = KEY_POWER | KEY_COMBO_OPEN_CLOSE;
  static constexpr uint32_t KEY_COMBO_DIRECTION_UP = KEY_DIRECTION | KEY_UP;
  static constexpr uint32_t KEY_COMBO_DIRECTION_DOWN = KEY_DIRECTION | KEY_DOWN;
  static constexpr auto VALID_KEY_COMBOS = {
      KEY_UP,
      KEY_DOWN,
      KEY_RAIN,
      KEY_POWER,
      KEY_DIRECTION,
      KEY_AUTO,
      KEY_4KEY_CLOSE,
      KEY_4KEY_OPEN,
      KEY_4KEY_OFF,
      KEY_4KEY_ON,
      KEY_COMBO_OPEN_CLOSE,
      KEY_COMBO_AUTO_UP,
      KEY_COMBO_AUTO_DOWN,
      KEY_COMBO_AUTO_DIRECTION,
      KEY_COMBO_AUTO_POWER,
      KEY_COMBO_AUTO_OPEN_CLOSE,
      KEY_COMBO_POWER_UP,
      KEY_COMBO_POWER_DOWN,
      KEY_COMBO_POWER_DIRECTION,
      KEY_COMBO_POWER_OPEN_CLOSE,
      KEY_COMBO_DIRECTION_UP,
      KEY_COMBO_DIRECTION_DOWN,
  };

  static constexpr uint32_t DEBOUNCE_DURATION_MS = 40;

  // Un-debounced (raw) key state from the previous scan.
  uint32_t bouncy_key_state_{0};
  // Time when `bouncy_key_state` was last modified.
  uint32_t bouncy_key_time_{0};
  // Debounced state of the keys from the previous scan.
  uint32_t last_key_state_{0};
  // True if the key press described by `last_key_state` is valid for dispatch, false if
  // the key press has already been handled or if no further processing should occur.
  bool last_key_valid_{false};
  // Time when `last_key_state` was last modified.
  uint32_t last_key_time_{0};
  // Number of times that AUTO+POWER have been pressed while AUTO remains held to specify
  // the action that should happen when AUTO is released.
  uint8_t pending_auto_power_action_{0};
  // Number of times that AUTO+DIRECTION have been pressed while AUTO remains held to specify
  // the action that should happen when AUTO is released.
  uint8_t pending_auto_direction_action_{0};
  // Number of times that AUTO+OPEN/CLOSE have been pressed while AUTO remains held to specify
  // the action that should happen when AUTO is released.
  uint8_t pending_auto_open_close_action_{0};
  // A pending action to be performed when all keys are released.
  enum class ReleaseAction : uint8_t {
    NONE = 0,
    RESTART,
    RADIO_TOGGLE,
  };
  ReleaseAction pending_release_action_{};

  // Indicator state.
  Indicator indicator_auto_{};
  Indicator indicator_rain_{};
  uint8_t indicator_cycle_{};

  void do_press_power_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::coordinator::command_auto_off();
        minuet::tone::play("?auto_off");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          minuet::coordinator::command_manual_power_off();
          minuet::tone::play("?power_off");
        } else {
          minuet::coordinator::command_manual_power_on();
          minuet::tone::play("?power_on");
        }
        break;
      default:
        break;
    }
  }

  void do_press_4key_on_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          const auto SPEED_CYCLE = {
              1,
              3,
              7,
              10,
          };
          const auto it =
              std::find_if(SPEED_CYCLE.begin(), SPEED_CYCLE.end(), [](int speed) { return speed > minuet_fan->speed; });
          const bool going_up = it != SPEED_CYCLE.end();
          minuet::coordinator::command_manual_fan_speed(going_up ? *it : *SPEED_CYCLE.begin());
          minuet::tone::play(going_up ? "?fan_speed_up" : "?fan_speed_min");
        } else {
          minuet::coordinator::command_manual_power_on();
          minuet::tone::play("?power_on");
        }
        break;
      default:
        break;
    }
  }

  void do_press_4key_off_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::coordinator::command_manual_power_off();
        minuet::tone::play("?power_off");
        break;
      default:
        break;
    }
  }

  void do_press_up_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::coordinator::command_auto_thermostat_temperature_change(1);
        minuet::tone::play("auto_temp_up");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          minuet::coordinator::command_manual_fan_speed(
              std::min(minuet_fan->speed + 1, minuet_fan->get_traits().supported_speed_count()));
          minuet::tone::play("?fan_speed_up");
        }
        break;
      default:
        break;
    }
  }

  void do_hold_up_() {
    switch (current_menu()) {
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          minuet::coordinator::command_manual_fan_speed(minuet_fan->get_traits().supported_speed_count());
          minuet::tone::play("?fan_speed_max");
        }
        break;
      default:
        break;
    }
  }

  void do_press_down_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::coordinator::command_auto_thermostat_temperature_change(-1);
        minuet::tone::play("auto_temp_down");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          minuet::coordinator::command_manual_fan_speed(std::max(minuet_fan->speed - 1, 1));
          minuet::tone::play("?fan_speed_down");
        }
        break;
      default:
        break;
    }
  }

  void do_hold_down_() {
    switch (current_menu()) {
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          minuet::coordinator::command_manual_fan_speed(1);
          minuet::tone::play("?fan_speed_min");
        }
        break;
      default:
        break;
    }
  }

  void do_press_open_close_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::tone::play("forbidden");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::coordinator::command_manual_lid_toggle();
        switch (minuet_lid->current_operation) {
          case esphome::cover::COVER_OPERATION_OPENING:
            minuet::tone::play("?lid_open");
            break;
          case esphome::cover::COVER_OPERATION_CLOSING:
            minuet::tone::play("?lid_close");
            break;
          case esphome::cover::COVER_OPERATION_IDLE:
            minuet::tone::play("?lid_stop");
            break;
        }
        break;
      default:
        break;
    }
  }

  void do_press_4key_open_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::tone::play("forbidden");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::coordinator::command_manual_lid_open();
        minuet::tone::play("?lid_open");
        break;
      default:
        break;
    }
  }

  void do_press_4key_close_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::tone::play("forbidden");
        break;
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::coordinator::command_manual_lid_close();
        minuet::tone::play("?lid_close");
        break;
      default:
        break;
    }
  }

  void do_press_direction_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet::fan::is_on()) {
          const bool exhaust = !minuet::fan::direction_is_exhaust(minuet_fan->direction);
          minuet::coordinator::command_fan_direction(exhaust);
          minuet::tone::play(exhaust ? "?fan_dir_air_out" : "?fan_dir_air_in");
        }
        break;
      default:
        break;
    }
  }

  void do_press_auto_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
        minuet::coordinator::command_auto_off();
        minuet::tone::play("?auto_off");
        break;
      case Menu::STANDARD_MANUAL:
        minuet::coordinator::command_auto_on();
        minuet::tone::play("?auto_on");
        break;
      case Menu::ENHANCED:
        if (minuet::auto_mode::is_active()) {
          minuet::coordinator::command_auto_off();
          minuet::tone::play("?auto_off");
        } else {
          minuet::coordinator::command_auto_on();
          minuet::tone::play("?auto_on");
        }
        break;
      default:
        break;
    }
  }

  void do_press_auto_up_() {
    switch (current_menu()) {
      case Menu::ENHANCED:
        minuet::coordinator::command_auto_thermostat_temperature_change(1);
        minuet::tone::play("auto_temp_up");
        break;
      default:
        break;
    }
  }

  void do_press_auto_down_() {
    switch (current_menu()) {
      case Menu::ENHANCED:
        minuet::coordinator::command_auto_thermostat_temperature_change(-1);
        minuet::tone::play("auto_temp_down");
        break;
      default:
        break;
    }
  }

  void do_press_auto_power_() {
    switch (current_menu()) {
      case Menu::ENHANCED:
        this->pending_auto_power_action_ += 1;
        break;
      default:
        break;
    }
  }

  void finish_auto_power_action_() {
    switch (this->pending_auto_power_action_) {
      case 0:
        return;
      case 1:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::AUTO);
        minuet::tone::play("setting_1");
        break;
      case 2:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::QUIET);
        minuet::tone::play("setting_2");
        break;
      case 3:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::OFF);
        minuet::tone::play("setting_3");
        break;
      case 4:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::MINIMUM);
        minuet::tone::play("setting_4");
        break;
      case 5:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::LOW);
        minuet::tone::play("setting_5");
        break;
      case 6:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::MEDIUM);
        minuet::tone::play("setting_6");
        break;
      case 7:
        minuet::coordinator::command_auto_thermostat_fan_mode(minuet::auto_mode::AutoFanMode::HIGH);
        minuet::tone::play("setting_7");
        break;
      default:
        minuet::tone::play("warn");
        break;
    }
    this->pending_auto_power_action_ = 0;
  }

  void do_press_auto_direction_() {
    switch (current_menu()) {
      case Menu::ENHANCED:
        this->pending_auto_direction_action_ += 1;
        break;
      default:
        break;
    }
  }

  void finish_auto_direction_action_() {
    switch (this->pending_auto_direction_action_) {
      case 0:
        return;
      case 1:
        minuet::coordinator::command_auto_fan_direction(minuet::auto_mode::AutoFanDirection::AUTO);
        minuet::tone::play("setting_1");
        break;
      case 2:
        minuet::coordinator::command_auto_fan_direction(minuet::auto_mode::AutoFanDirection::AIR_OUT);
        minuet::tone::play("setting_2");
        break;
      case 3:
        minuet::coordinator::command_auto_fan_direction(minuet::auto_mode::AutoFanDirection::AIR_IN);
        minuet::tone::play("setting_3");
        break;
      default:
        minuet::tone::play("warn");
        break;
    }
    this->pending_auto_direction_action_ = 0;
  }

  void do_press_auto_open_close_() {
    switch (current_menu()) {
      case Menu::ENHANCED:
        this->pending_auto_open_close_action_ += 1;
        break;
      default:
        break;
    }
  }

  void finish_auto_open_close_action_() {
    switch (this->pending_auto_open_close_action_) {
      case 0:
        return;
      case 1:
        minuet::coordinator::command_auto_lid_position(minuet::auto_mode::AutoLidPosition::AUTO);
        minuet::tone::play("setting_1");
        break;
      case 2:
        minuet::coordinator::command_auto_lid_position(minuet::auto_mode::AutoLidPosition::OPEN);
        minuet::tone::play("setting_2");
        break;
      case 3:
        minuet::coordinator::command_auto_lid_position(minuet::auto_mode::AutoLidPosition::CLOSED);
        minuet::tone::play("setting_3");
        break;
      default:
        minuet::tone::play("warn");
        break;
    }
    this->pending_auto_open_close_action_ = 0;
  }

  void do_hold_auto_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::ENHANCED:
        minuet::coordinator::command_auto_reset();
        minuet::tone::play("auto_reset");
        break;
      default:
        break;
    }
  }

  void do_press_rain_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        if (minuet_rain_safety_lock_latched->state || !minuet_rain_sensor_enabled->state) {
          minuet_rain_sensor_reset->press();
          minuet::tone::play("?rain_on");
        } else {
          minuet_rain_sensor_enabled->turn_off();
          minuet::tone::play("?rain_off");
        }
        break;
      default:
        break;
    }
  }

  void do_press_direction_up_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::accessory::dispatch_handler(&minuet::accessory::Accessory::handle_up);
        break;
      default:
        break;
    }
  }

  void do_press_direction_down_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::accessory::dispatch_handler(&minuet::accessory::Accessory::handle_down);
        break;
      default:
        break;
    }
  }

  void do_hold_direction_() {
    switch (current_menu()) {
      case Menu::STANDARD_AUTO:
      case Menu::STANDARD_MANUAL:
      case Menu::ENHANCED:
        minuet::accessory::dispatch_handler(&minuet::accessory::Accessory::handle_toggle);
        break;
      default:
        break;
    }
  }

  void do_action_use_enhanced_controls_() {
    // Available in all menu modes.
    minuet_keypad_controls->make_call().set_index(0).perform();
    minuet::tone::play("!controls_enhanced");
  }

  void do_action_use_standard_controls_() {
    // Available in all menu modes.
    minuet_keypad_controls->make_call().set_index(1).perform();
    minuet::tone::play("!controls_standard");
  }

  void do_action_keypad_indicators_toggle_() {
    // Available in all menu modes.
    minuet_keypad_indicators->make_call().select_next(true).perform();
    const bool can_sleep = can_indicators_sleep();
    const auto &pattern = can_sleep ? INDICATOR_PATTERN_CONFIRM_2 : INDICATOR_PATTERN_CONFIRM_3;
    this->indicator_auto_.start_pattern(pattern, 0);
    this->indicator_rain_.start_pattern(pattern, 0);
    if (can_sleep) {
      this->sleep_indicators();
    }
    minuet::tone::play("?keypad_indicators");
  }

  void do_action_radio_toggle_() {
    // Available in all menu modes.
#ifdef MINUET_RADIO
    const bool new_state = minuet::radio::toggle_radios_jointly();
    minuet::tone::play(new_state ? "!radio_on" : "!radio_off");
#endif
  }

  void do_action_radio_pairing_() {
    // Available in all menu modes.
#ifdef MINUET_RADIO
    minuet::radio::turn_on_and_pair();
    minuet::tone::play("!radio_on_pair");
#endif
  }

  void do_action_power_on_behavior_toggle_() {
    // Available in all menu modes.
    minuet_power_on_behavior->make_call().select_next(true).perform();
    minuet::tone::play(minuet_power_on_behavior->active_index() == 1 ? "!power_on_restore" : "!power_on_default");
  };

  void do_action_manual_safety_lock_toggle_() {
    // Available in all menu modes.
    minuet_manual_safety_lock->toggle();
    minuet::tone::play(minuet_manual_safety_lock->state ? "!lock_on" : "!lock_off");
  }

  void do_action_restart_() {
    // Available in all menu modes.
    minuet_restart_after_delay->execute();
    minuet::tone::play("!restart");
  }

  void do_action_factory_reset_() {
    // Available in all menu modes.
    minuet_factory_reset_after_delay->execute();
    minuet::tone::play("!factory_reset");
  }

  static void log_press_(const char *key) {
    ESP_LOGD(minuet::TAG, "Press: %s", key);
  }

  void dispatch_press_(uint32_t key_state, uint32_t press_duration_ms) {
    constexpr uint32_t MIN_PRESS_DURATION_MS = 100;
    constexpr uint32_t MAX_PRESS_DURATION_MS = 800;
    if (press_duration_ms >= MIN_PRESS_DURATION_MS && press_duration_ms <= MAX_PRESS_DURATION_MS) {
      switch (key_state) {
        case KEY_UP:
          log_press_("KEY_UP");
          this->do_press_up_();
          return;
        case KEY_DOWN:
          log_press_("KEY_DOWN");
          this->do_press_down_();
          return;
        case KEY_RAIN:
          log_press_("KEY_RAIN");
          this->do_press_rain_();
          return;
        case KEY_POWER:
          log_press_("KEY_POWER");
          this->do_press_power_();
          return;
        case KEY_DIRECTION:
          log_press_("KEY_DIRECTION");
          this->do_press_direction_();
          return;
        case KEY_AUTO:
          log_press_("KEY_AUTO");
          this->do_press_auto_();
          return;
        case KEY_4KEY_ON:
          log_press_("KEY_4KEY_ON");
          this->do_press_4key_on_();
          return;
        case KEY_4KEY_OFF:
          log_press_("KEY_4KEY_OFF");
          this->do_press_4key_off_();
          return;
        case KEY_4KEY_OPEN:
          log_press_("KEY_4KEY_OPEN");
          this->do_press_4key_open_();
          return;
        case KEY_4KEY_CLOSE:
          log_press_("KEY_4KEY_CLOSE");
          this->do_press_4key_close_();
          return;
        case KEY_COMBO_OPEN_CLOSE:
          log_press_("KEY_COMBO_OPEN_CLOSE");
          this->do_press_open_close_();
          return;
        case KEY_COMBO_AUTO_UP:
          log_press_("KEY_COMBO_AUTO_UP");
          this->do_press_auto_up_();
          return;
        case KEY_COMBO_AUTO_DOWN:
          log_press_("KEY_COMBO_AUTO_DOWN");
          this->do_press_auto_down_();
          return;
        case KEY_COMBO_AUTO_DIRECTION:
          log_press_("KEY_COMBO_AUTO_DIRECTION");
          this->do_press_auto_direction_();
          return;
        case KEY_COMBO_AUTO_OPEN_CLOSE:
          log_press_("KEY_COMBO_AUTO_OPEN_CLOSE");
          this->do_press_auto_open_close_();
          return;
        case KEY_COMBO_AUTO_POWER:
          log_press_("KEY_COMBO_AUTO_POWER");
          this->do_press_auto_power_();
          return;
        case KEY_COMBO_DIRECTION_UP:
          log_press_("KEY_COMBO_DIRECTION_UP");
          this->do_press_direction_up_();
          return;
        case KEY_COMBO_DIRECTION_DOWN:
          log_press_("KEY_COMBO_DIRECTION_DOWN");
          this->do_press_direction_down_();
          return;
      }
    }
  }

  static void log_hold_(const char *key) {
    ESP_LOGD(minuet::TAG, "Hold: %s", key);
  }

  bool dispatch_hold_(uint32_t key_state, uint32_t hold_duration_ms) {
    constexpr uint32_t MIN_HOLD_UP_DOWN_DURATION_MS = 1000;
    constexpr uint32_t MIN_HOLD_DIRECTION_DURATION_MS = 1000;
    constexpr uint32_t MIN_HOLD_AUTO_DURATION_MS = 3000;
    constexpr uint32_t MIN_HOLD_ACTION_1_DURATION_MS = 5000;
    constexpr uint32_t MIN_HOLD_ACTION_2_DURATION_MS = 8000;
    constexpr uint32_t MIN_HOLD_ACTION_3_DURATION_MS = 15000;
    switch (key_state) {
      case KEY_UP:
        if (hold_duration_ms >= MIN_HOLD_UP_DOWN_DURATION_MS) {
          log_hold_("KEY_UP");
          this->do_hold_up_();
          return true;
        }
        break;
      case KEY_DOWN:
        if (hold_duration_ms >= MIN_HOLD_UP_DOWN_DURATION_MS) {
          log_hold_("KEY_DOWN");
          this->do_hold_down_();
          return true;
        }
        break;
      case KEY_DIRECTION:
        if (hold_duration_ms >= MIN_HOLD_DIRECTION_DURATION_MS) {
          log_hold_("KEY_DIRECTION");
          this->do_hold_direction_();
          return true;
        }
        break;
      case KEY_AUTO:
        if (hold_duration_ms >= MIN_HOLD_AUTO_DURATION_MS) {
          log_hold_("KEY_AUTO");
          this->do_hold_auto_();
          return true;
        }
        break;
      case KEY_COMBO_AUTO_UP:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS) {
          log_hold_("KEY_COMBO_AUTO_UP");
          this->do_action_use_enhanced_controls_();
          return true;
        }
        break;
      case KEY_COMBO_AUTO_DOWN:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS) {
          log_hold_("KEY_COMBO_AUTO_DOWN");
          this->do_action_use_standard_controls_();
          return true;
        }
        break;
      case KEY_COMBO_POWER_DOWN:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS && this->pending_release_action_ == ReleaseAction::NONE) {
          log_hold_("KEY_COMBO_POWER_DOWN~1");
          minuet::tone::play("!hold_action_1");
          this->pending_release_action_ = ReleaseAction::RADIO_TOGGLE;
          return false;
        }
        if (hold_duration_ms >= MIN_HOLD_ACTION_2_DURATION_MS) {
          log_hold_("KEY_COMBO_POWER_DOWN~2");
          this->pending_release_action_ = ReleaseAction::NONE;
          this->do_action_radio_pairing_();
          return true;
        }
        break;
      case KEY_COMBO_POWER_UP:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS) {
          log_hold_("KEY_COMBO_POWER_UP");
          this->do_action_power_on_behavior_toggle_();
          return true;
        }
        break;
      case KEY_COMBO_POWER_DIRECTION:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS) {
          log_hold_("KEY_COMBO_POWER_DIRECTION");
          this->do_action_manual_safety_lock_toggle_();
          return true;
        }
        break;
      case KEY_COMBO_POWER_OPEN_CLOSE:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS) {
          log_hold_("KEY_COMBO_POWER_OPEN_CLOSE");
          this->do_action_keypad_indicators_toggle_();
          return true;
        }
        break;
      case KEY_POWER:
        if (hold_duration_ms >= MIN_HOLD_ACTION_1_DURATION_MS && this->pending_release_action_ == ReleaseAction::NONE) {
          log_hold_("KEY_POWER~1");
          minuet::tone::play("!hold_action_1");
          this->pending_release_action_ = ReleaseAction::RESTART;
          return false;
        }
        if (hold_duration_ms >= MIN_HOLD_ACTION_3_DURATION_MS) {
          log_hold_("KEY_POWER~3");
          this->pending_release_action_ = ReleaseAction::NONE;
          this->do_action_factory_reset_();
          return true;
        }
        break;
    }
    return false;
  }

  void dispatch_release_() {
    // Handle pending multiple-tap key presses after the modifier has been released
    this->finish_auto_power_action_();
    this->finish_auto_direction_action_();
    this->finish_auto_open_close_action_();

    // Handle release from multi-stage hold actions.
    switch (this->pending_release_action_) {
      case ReleaseAction::RESTART:
        this->do_action_restart_();
        break;
      case ReleaseAction::RADIO_TOGGLE:
        this->do_action_radio_toggle_();
        break;
      default:
        break;
    }
    this->pending_release_action_ = ReleaseAction::NONE;
  }
};

void Keypad::poll_keys() {
  // Scan the keypad.  All of the pins have internal pull-up resistors and some
  // of them are used as both inputs and outputs.
  constexpr uint8_t XIO_PIN_R1 = 6;
  constexpr uint8_t XIO_PIN_R2 = 3;
  constexpr uint8_t XIO_PIN_C1 = 7;
  constexpr uint8_t XIO_PIN_C2 = 5;
  constexpr uint8_t XIO_PIN_C3 = 2;
  constexpr uint8_t XIO_PIN_C4 = 4;

  // Read the keypad row pins themselves while they are undriven to detect OPEN and CLOSE
  // on the 4 key keypad.
  minuet_xio->loop();
  const bool key_r1_gnd = !minuet_xio->digital_read(XIO_PIN_R1);
  const bool key_r2_gnd = !minuet_xio->digital_read(XIO_PIN_R2);

  // Drive keypad row 1 low and read the columns.
  minuet_xio->pin_mode(XIO_PIN_R1, esphome::gpio::FLAG_OUTPUT);
  minuet_xio->loop();
  const bool key_r1_c1 = !minuet_xio->digital_read(XIO_PIN_C1);
  const bool key_r1_c2 = !minuet_xio->digital_read(XIO_PIN_C2);
  const bool key_r1_c3 = !minuet_xio->digital_read(XIO_PIN_C3);
  const bool key_r1_c4 = !minuet_xio->digital_read(XIO_PIN_C4);
  minuet_xio->pin_mode(XIO_PIN_R1, esphome::gpio::FLAG_INPUT);

  // Drive keypad row 2 low and read the columns.
  minuet_xio->pin_mode(XIO_PIN_R2, esphome::gpio::FLAG_OUTPUT);
  minuet_xio->loop();
  const bool key_r2_c1 = !minuet_xio->digital_read(XIO_PIN_C1);
  const bool key_r2_c2 = !minuet_xio->digital_read(XIO_PIN_C2);
  const bool key_r2_c3 = !minuet_xio->digital_read(XIO_PIN_C3);
  const bool key_r2_c4 = !minuet_xio->digital_read(XIO_PIN_C4);
  minuet_xio->pin_mode(XIO_PIN_R2, esphome::gpio::FLAG_INPUT);

  // Combine all of the key states into a single value with one bit per key
  // that represents the complete state of the keypad.
  const uint32_t key_state = (key_r1_c2 ? KEY_UP : 0) | (key_r1_c3 ? KEY_DOWN : 0) | (key_r1_c4 ? KEY_RAIN : 0) |
      (key_r2_c2 ? KEY_POWER : 0) | (key_r2_c3 ? KEY_DIRECTION : 0) | (key_r2_c4 ? KEY_AUTO : 0) |
      (key_r1_gnd ? KEY_4KEY_CLOSE : 0) | (key_r2_gnd ? KEY_4KEY_OPEN : 0) | (key_r1_c1 ? KEY_4KEY_OFF : 0) |
      (key_r2_c1 ? KEY_4KEY_ON : 0);

  // Debounce the keys.
  const uint32_t now = esphome::millis();
  if (key_state != this->bouncy_key_state_) {
    this->bouncy_key_state_ = key_state;
    this->bouncy_key_time_ = now;
    return;  // wait for key state to be debounced before processing it
  }
  if (now - bouncy_key_time_ < DEBOUNCE_DURATION_MS) {
    return;  // wait for key state to be debounced before processing it
  }

  if (key_state) {
    this->wake_indicators();
  } else {
    this->dispatch_release_();
  }

  // Dispatch key presses
  bool key_valid = std::find(VALID_KEY_COMBOS.begin(), VALID_KEY_COMBOS.end(), key_state) != VALID_KEY_COMBOS.end();
  if (key_state == 0 && this->last_key_valid_) {
    // A key was released
    this->dispatch_press_(this->last_key_state_, now - this->last_key_time_);
  } else if (key_valid && this->last_key_state_ == 0) {
    // A key was pressed on its own
    this->last_key_time_ = now;
  } else if (key_valid && key_state != this->last_key_state_ && key_state == (key_state | this->last_key_state_)) {
    // A key was pressed that adds to a previously pressed key to form a combo
    this->last_key_time_ = now;
  } else if (key_valid && this->last_key_valid_ && key_state == this->last_key_state_) {
    // A key is being held
    if (this->dispatch_hold_(key_state, now - this->last_key_time_)) {
      key_valid = false;  // cancel further processing of this key
    }
  } else if (key_valid && key_state == KEY_AUTO && this->last_key_valid_ && (this->last_key_state_ & KEY_AUTO) != 0) {
    // A key that was previously combined with auto has been released while auto remains held
    this->dispatch_press_(this->last_key_state_, now - this->last_key_time_);
    key_valid = false;  // cancel processing of the modifier itself
  } else if (key_valid && key_state == KEY_DIRECTION && this->last_key_valid_ &&
      (this->last_key_state_ & KEY_DIRECTION) != 0) {
    // A key that was previously combined with direction has been released while direction remains held
    this->dispatch_press_(this->last_key_state_, now - this->last_key_time_);
    key_valid = false;  // cancel processing of the modifier itself
  } else if (key_valid) {
    // A different key is pressed now than was pressed before and does not form a valid combo sequence
    key_valid = false;  // cancel further processing of this key
  }
  this->last_key_state_ = key_state;
  this->last_key_valid_ = key_valid;
}

void Keypad::poll_indicators() {
  static const IndicatorPattern PAIRING_MODE_PATTERNS[] = {
      INDICATOR_PATTERN_OFF,
      INDICATOR_PATTERN_ATTENTION,
  };
  this->indicator_auto_.set_condition(
      []() -> uint8_t {
#ifdef MINUET_RADIO
        if (minuet_radio_pairing_mode->state)
          return 1;
#endif
        return 0;
      }(),
      PAIRING_MODE_PATTERNS, 1);

  static const IndicatorPattern MODE_CHANGE_PATTERNS[] = {
      INDICATOR_PATTERN_OFF,
      INDICATOR_PATTERN_BLINK_BRIEFLY,
  };
  this->indicator_auto_.set_condition(
      []() -> uint8_t { return minuet::coordinator::was_last_mode_change_manual_override() ? 1 : 0; }(),
      MODE_CHANGE_PATTERNS, 2);

  static const IndicatorPattern AUTO_MODE_PATTERNS[] = {
      INDICATOR_PATTERN_OFF,
      INDICATOR_PATTERN_ON,
  };
  this->indicator_auto_.set_condition(
      []() -> uint8_t {
        if (minuet::auto_mode::is_active())
          return 1;
        return 0;
      }(),
      AUTO_MODE_PATTERNS, 3);

  static const IndicatorPattern RAIN_PATTERNS[] = {
      INDICATOR_PATTERN_OFF,
      INDICATOR_PATTERN_ON,
      INDICATOR_PATTERN_BLINK,
  };
  this->indicator_rain_.set_condition(
      []() -> uint8_t {
        if (!minuet_rain_sensor_enabled->state)
          return 1;
        if (minuet_rain_safety_lock_latched->state)
          return 2;
        return 0;
      }(),
      RAIN_PATTERNS, 1);

  const bool can_sleep = can_indicators_sleep();
  this->indicator_cycle_ += 1;
  minuet_keypad_indicator_auto->set_state(this->indicator_auto_.poll(this->indicator_cycle_, can_sleep));
  minuet_keypad_indicator_rain->set_state(this->indicator_rain_.poll(this->indicator_cycle_, can_sleep));
}

void Keypad::wake_indicators() {
  this->indicator_auto_.wake();
  this->indicator_rain_.wake();
}

void Keypad::sleep_indicators() {
  this->indicator_auto_.sleep();
  this->indicator_rain_.sleep();
}

Keypad keypad{};

}  // namespace keypad
}  // namespace minuet
