// MINUET CORE HELPERS
#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"

#include <cstdint>
#include <map>

namespace minuet {

const char *const TAG = "minuet";

uint8_t transient_operation_depth = 0;

// Returns true if the current operation was initiated by transient operations that should
// not update the persistent state, such as synchronizing the fan with the automatic thermostat.
bool is_transient_operation() {
  return transient_operation_depth != 0;
}

// Runs a function as a transient operation.
template <typename T>
void perform_transient_operation(T func) {
  transient_operation_depth++;
  func();
  transient_operation_depth--;
}

// Records the state of the fan and lid as it was most recently set by the user, disregarding transient operations.
struct PersistentState {
  bool fan_on : 1 {false};
  unsigned fan_speed : 4 {1};
  bool fan_exhaust : 1 {true};
  bool lid_open : 1 {false};

  // The storage hack is needed because ESPHome global declarations cannot refer to types in user-defined include files.
  using Storage = typename std::remove_reference<decltype(*minuet_persistent_state_raw)>::type::value_type;
  constexpr Storage& to_storage() { return reinterpret_cast<Storage&>(*this); }
  static constexpr PersistentState& from_storage(Storage& storage) { return reinterpret_cast<PersistentState&>(storage); }
} __attribute__((packed));

static_assert(sizeof(PersistentState) == sizeof(PersistentState::Storage));

PersistentState& persistent_state() {
  return PersistentState::from_storage(minuet_persistent_state_raw->value());
}

bool cover_is_open_or_opening(esphome::cover::Cover* cover) {
  return (cover->current_operation == COVER_OPERATION_IDLE && cover->position == COVER_OPEN)
      || cover->current_operation == COVER_OPERATION_OPENING;
}

constexpr bool fan_direction_is_exhaust(esphome::fan::FanDirection direction) {
  return direction == esphome::fan::FanDirection::REVERSE;
}

constexpr esphome::fan::FanDirection fan_direction(bool exhaust) {
  return exhaust ? esphome::fan::FanDirection::REVERSE : esphome::fan::FanDirection::FORWARD;
}

// The lid behavior when the thermostat is enabled.
enum class LidMode : uint8_t {
  AUTO = 0,
  OPEN = 1,
  CLOSED = 2,
};

esphome::thermostat::ThermostatPresetEntry make_thermostat_preset_entry(
    esphome::climate::ClimatePreset preset,
    esphome::select::Select* fan_mode_setting,
    esphome::number::Number* temperature_setting) {
  constexpr const esphome::climate::ClimateFanMode FAN_MODES[] = {
    esphome::climate::ClimateFanMode::CLIMATE_FAN_AUTO,
    esphome::climate::ClimateFanMode::CLIMATE_FAN_QUIET,
    esphome::climate::ClimateFanMode::CLIMATE_FAN_LOW,
    esphome::climate::ClimateFanMode::CLIMATE_FAN_OFF,
  };
  esphome::thermostat::ThermostatClimateTargetTempConfig config(temperature_setting->state);
  config.set_mode(esphome::climate::ClimateMode::CLIMATE_MODE_COOL);
  config.set_fan_mode(FAN_MODES[fan_mode_setting->active_index().value_or(0)]);
  return { preset, config };
}

void publish_thermostat_presets() {
  minuet_thermostat->set_preset_config({
      make_thermostat_preset_entry(esphome::climate::ClimatePreset::CLIMATE_PRESET_HOME,
          minuet_thermostat_preset_home_fan_mode_setting,
          minuet_thermostat_preset_home_temperature_setting),
      make_thermostat_preset_entry(esphome::climate::ClimatePreset::CLIMATE_PRESET_SLEEP,
          minuet_thermostat_preset_sleep_fan_mode_setting,
          minuet_thermostat_preset_sleep_temperature_setting),
      make_thermostat_preset_entry(esphome::climate::ClimatePreset::CLIMATE_PRESET_AWAY,
          minuet_thermostat_preset_away_fan_mode_setting,
          minuet_thermostat_preset_away_temperature_setting),
      make_thermostat_preset_entry(esphome::climate::ClimatePreset::CLIMATE_PRESET_ECO,
          minuet_thermostat_preset_eco_fan_mode_setting,
          minuet_thermostat_preset_eco_temperature_setting),
  });
}

LidMode get_thermostat_preset_lid_mode_from_setting(esphome::select::Select* lid_mode_setting) {
  return minuet::LidMode(lid_mode_setting->active_index().value_or(0));
}

LidMode get_thermostat_preset_lid_mode(ClimatePreset preset) {
  switch (preset) {
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_HOME:
      return get_thermostat_preset_lid_mode_from_setting(minuet_thermostat_preset_home_lid_mode_setting);
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_SLEEP:
      return get_thermostat_preset_lid_mode_from_setting(minuet_thermostat_preset_sleep_lid_mode_setting);
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_AWAY:
      return get_thermostat_preset_lid_mode_from_setting(minuet_thermostat_preset_away_lid_mode_setting);
    case esphome::climate::ClimatePreset::CLIMATE_PRESET_ECO:
      return get_thermostat_preset_lid_mode_from_setting(minuet_thermostat_preset_eco_lid_mode_setting);
    default:
      return LidMode::AUTO;
  }
}

} // namespace minuet
