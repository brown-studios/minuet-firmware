// MINUET AUTO MODE PRESETS HEADER

#pragma once

#include <array>

#include "controllers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/template/climate/template_climate.h"

namespace minuet {
namespace auto_mode {

constexpr const char *CUSTOM_PRESET_DEFAULT = "Default";

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
  auto thermostat_call =
      minuet_auto_thermostat->make_call()
          .set_mode(thermostat_on ? esphome::climate::CLIMATE_MODE_COOL : esphome::climate::CLIMATE_MODE_OFF)
          .set_target_temperature(thermostat_target_temperature);
  set_climate_call_fan_mode(thermostat_call, thermostat_fan_mode).perform();
  set_thermostat_fan_speed(get_preset_setting<int, esphome::number::Number>(
      preset, MINUET_PRESET_OPTIONS(thermostat_fan_speed), [] { return get_thermostat_default_fan_speed(); },
      [](auto option) { return std::clamp(int(option->state), 1, 10); }));
#endif

  // Humidistat settings
#ifdef MINUET_AUTO_HUMIDISTAT
  const auto humidistat_on = get_preset_setting<bool, esphome::switch_::Switch>(
      preset, MINUET_PRESET_OPTIONS(humidistat_on), [] { return get_humidistat_default_on(); },
      [](auto option) { return option->state; });
  minuet_auto_humidistat->make_call()
      .set_mode(humidistat_on ? esphome::climate::CLIMATE_MODE_AUTO : esphome::climate::CLIMATE_MODE_OFF)
      .perform();
#endif

  // CO2 monitor settings
#ifdef MINUET_AUTO_CO2_MONITOR
  const auto co2_monitor_on = get_preset_setting<bool, esphome::switch_::Switch>(
      preset, MINUET_PRESET_OPTIONS(co2_monitor_on), [] { return get_co2_monitor_default_on(); },
      [](auto option) { return option->state; });
  minuet_auto_co2_monitor->make_call()
      .set_mode(co2_monitor_on ? esphome::climate::CLIMATE_MODE_AUTO : esphome::climate::CLIMATE_MODE_OFF)
      .perform();
#endif

  // AQI monitor settings
#ifdef MINUET_AUTO_AQI_MONITOR
  const auto aqi_monitor_on = get_preset_setting<bool, esphome::switch_::Switch>(
      preset, MINUET_PRESET_OPTIONS(aqi_monitor_on), [] { return get_aqi_monitor_default_on(); },
      [](auto option) { return option->state; });
  minuet_auto_aqi_monitor->make_call()
      .set_mode(aqi_monitor_on ? esphome::climate::CLIMATE_MODE_AUTO : esphome::climate::CLIMATE_MODE_OFF)
      .perform();
#endif
}

std::optional<Preset> get_climate_call_preset(const esphome::climate::ClimateCall &call) {
  if (call.get_preset().has_value()) {
    switch (call.get_preset().value()) {
      case esphome::climate::CLIMATE_PRESET_HOME:
        return Preset::HOME;
      case esphome::climate::CLIMATE_PRESET_SLEEP:
        return Preset::SLEEP;
      case esphome::climate::CLIMATE_PRESET_AWAY:
        return Preset::AWAY;
      case esphome::climate::CLIMATE_PRESET_ECO:
        return Preset::ECO;
      default:
        break;
    }
  }
  if (call.has_custom_preset() && call.get_custom_preset() == CUSTOM_PRESET_DEFAULT) {
    return Preset::DEFAULT;
  }
  return {};
}

void set_active_preset_from_climate_call(const esphome::climate::ClimateCall &call) {
  std::optional<Preset> preset = get_climate_call_preset(call);
  if (preset.has_value()) {
    set_active_preset(preset.value());
  }
}

void publish_climate_preset(esphome::template_::TemplateClimate *climate, esphome::climate::ClimatePreset preset) {
  if (climate->preset != preset) {
    climate->set_preset(preset);
    climate->publish_state();
  }
}

void publish_custom_preset(esphome::template_::TemplateClimate *climate, const char *custom_preset) {
  if (climate->get_custom_preset() != custom_preset) {
    climate->set_custom_preset(custom_preset);
    climate->publish_state();
  }
}

void publish_preset(esphome::template_::TemplateClimate *climate, Preset preset) {
  switch (preset) {
    case Preset::DEFAULT:
      publish_custom_preset(climate, CUSTOM_PRESET_DEFAULT);
      break;
    case Preset::HOME:
      publish_climate_preset(climate, esphome::climate::CLIMATE_PRESET_HOME);
      break;
    case Preset::SLEEP:
      publish_climate_preset(climate, esphome::climate::CLIMATE_PRESET_SLEEP);
      break;
    case Preset::AWAY:
      publish_climate_preset(climate, esphome::climate::CLIMATE_PRESET_AWAY);
      break;
    case Preset::ECO:
      publish_climate_preset(climate, esphome::climate::CLIMATE_PRESET_ECO);
      break;
  }
}

void publish_preset() {
  const auto preset = get_active_preset();

#ifdef MINUET_AUTO_THERMOSTAT
  publish_preset(minuet_auto_thermostat, preset);
#endif
#ifdef MINUET_AUTO_HUMIDISTAT
  publish_preset(minuet_auto_humidistat, preset);
#endif
#ifdef MINUET_AUTO_CO2_MONITOR
  publish_preset(minuet_auto_co2_monitor, preset);
#endif
#ifdef MINUET_AUTO_AQI_MONITOR
  publish_preset(minuet_auto_aqi_monitor, preset);
#endif
}

}  // namespace auto_mode
}  // namespace minuet
