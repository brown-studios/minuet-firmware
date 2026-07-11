// MINUET AUTO MODE PRESETS HEADER

#pragma once

#include <array>

#include "controllers.h"

namespace minuet {
namespace auto_mode {

enum class Preset : uint8_t {
  DEFAULT = 0,
  HOME = 1,
  SLEEP = 2,
  AWAY = 3,
  ECO = 4,
};

Preset get_active_preset() {
  return Preset(minuet_auto_preset->active_index().value_or(0));
}

void set_active_preset(Preset preset) {
  minuet_auto_preset->make_call().set_index(size_t(preset)).perform();
}

template<typename TValue, typename TOption>
inline TValue get_preset_setting(Preset preset, std::array<TOption *, 4> options, TValue (*get_default_value)(),
    TValue (*get_option_value)(TOption *option)) {
  return preset == Preset::DEFAULT ? get_default_value() : get_option_value(options[size_t(preset) - 1]);
}

#define MINUET_PRESET_OPTIONS(id) \
  {minuet_auto_preset_home_##id, minuet_auto_preset_sleep_##id, minuet_auto_preset_away_##id, \
      minuet_auto_preset_eco_##id}

void apply_preset() {
  const auto preset = get_active_preset();

  // Common settings
  set_auto_fan_direction(get_preset_setting<AutoFanDirection, esphome::select::Select>(
      preset, MINUET_PRESET_OPTIONS(fan_direction), [] { return AutoFanDirection::AUTO; },
      [](auto option) { return AutoFanDirection(option->active_index().value_or(0)); }));

  set_auto_lid_position(get_preset_setting<AutoLidPosition, esphome::select::Select>(
      preset, MINUET_PRESET_OPTIONS(lid_position), [] { return AutoLidPosition::AUTO; },
      [](auto option) { return AutoLidPosition(option->active_index().value_or(0)); }));

  // Thermostat settings
#ifdef MINUET_AUTO_THERMOSTAT
  const auto thermostat_on = get_preset_setting<bool, esphome::switch_::Switch>(
      preset, MINUET_PRESET_OPTIONS(thermostat_on), [] { return get_thermostat_default_on(); },
      [](auto option) { return option->state; });
  const auto thermostat_target_temperature = get_preset_setting<float, esphome::number::Number>(
      preset, MINUET_PRESET_OPTIONS(thermostat_target_temperature),
      [] { return get_thermostat_default_target_temperature(); }, [](auto option) { return option->state; });
  const auto thermostat_fan_mode = get_preset_setting<AutoFanMode, esphome::select::Select>(
      preset, MINUET_PRESET_OPTIONS(thermostat_fan_mode), [] { return get_thermostat_default_fan_mode(); },
      [](auto option) { return AutoFanMode(option->active_index().value_or(0)); });
  auto call = minuet_auto_thermostat->make_call()
                  .set_mode(thermostat_on ? esphome::climate::CLIMATE_MODE_COOL : esphome::climate::CLIMATE_MODE_OFF)
                  .set_target_temperature(thermostat_target_temperature);
  set_climate_call_fan_mode(call, thermostat_fan_mode).perform();
  set_thermostat_fan_speed(get_preset_setting<int, esphome::number::Number>(
      preset, MINUET_PRESET_OPTIONS(thermostat_fan_speed), [] { return get_thermostat_default_fan_speed(); },
      [](auto option) { return std::clamp(int(option->state), 1, 10); }));
#endif

  // Humidistat settings
#ifdef MINUET_AUTO_HUMIDISTAT
#endif

  // CO2 monitor settings
#ifdef MINUET_AUTO_CO2_MONITOR
#endif

  // Air quality monitor settings
#ifdef MINUET_AUTO_AIR_QUALITY_MONITOR
#endif
}

}  // namespace auto_mode
}  // namespace minuet
