// MINUET LIGHT ACCESSORY HEADER

#pragma once

#include <algorithm>
#include <initializer_list>

#include "esphome/components/light/light_state.h"
#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace accessory {
namespace light {

using WRGBColor = uint32_t;

struct FloatColor {
  float w, r, g, b;
};

FloatColor to_float_color(WRGBColor wrgb) {
  return FloatColor{
      .w = ((wrgb >> 24) & 0xff) / 255.f,
      .r = ((wrgb >> 16) & 0xff) / 255.f,
      .g = ((wrgb >> 8) & 0xff) / 255.f,
      .b = (wrgb & 0xff) / 255.f,
  };
}

bool is_similar_color(FloatColor x, FloatColor y) {
  const float EPSILON = 0.02f;
  return std::fabs(x.w - y.w) < EPSILON && std::fabs(x.r - y.r) < EPSILON && std::fabs(x.g - y.g) < EPSILON &&
      std::fabs(x.b - y.b) < EPSILON;
}

enum class ModeType {
  WHITE = 0,
  COLOR,
  EFFECT,
};

struct Mode {
  ModeType type;
  union {
    WRGBColor wrgb;
    const char *effect;
  };
};

esphome::light::LightCall make_call_with_white(esphome::light::LightState *light) {
  auto call = light->make_call();
  if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB_WHITE)) {
    call.set_color_mode(esphome::light::ColorMode::RGB_WHITE).set_color_brightness(0.f).set_rgbw(1.f, 1.f, 1.f, 1.f);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::WHITE)) {
    call.set_color_mode(esphome::light::ColorMode::WHITE).set_white(1.f);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB)) {
    call.set_color_mode(esphome::light::ColorMode::RGB).set_rgb(1.f, 1.f, 1.f);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::BRIGHTNESS)) {
    call.set_color_mode(esphome::light::ColorMode::BRIGHTNESS);
  }
  return call.set_effect("none");
}

esphome::light::LightCall make_call_with_color(esphome::light::LightState *light, FloatColor color) {
  auto call = light->make_call();
  if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB_WHITE)) {
    call.set_color_mode(esphome::light::ColorMode::RGB_WHITE)
        .set_color_brightness(1.f)
        .set_rgbw(color.r, color.g, color.b, color.w);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB)) {
    call.set_color_mode(esphome::light::ColorMode::RGB).set_rgb(color.r, color.g, color.b);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::BRIGHTNESS)) {
    call.set_color_mode(esphome::light::ColorMode::BRIGHTNESS);
  }
  return call.set_effect("none");
}

esphome::light::LightCall make_call_with_color(esphome::light::LightState *light, WRGBColor wrgb) {
  return make_call_with_color(light, to_float_color(wrgb));
}

esphome::light::LightCall make_call_with_effect(esphome::light::LightState *light, const char *effect) {
  return light->make_call().set_effect(effect);
}

esphome::light::LightCall make_call_with_mode(esphome::light::LightState *light, const Mode &mode) {
  switch (mode.type) {
    default:
    case ModeType::WHITE:
      return make_call_with_white(light);
    case ModeType::COLOR:
      return make_call_with_color(light, mode.wrgb);
    case ModeType::EFFECT:
      return make_call_with_effect(light, mode.effect);
  }
}

void turn_off(esphome::light::LightState *light) {
  ESP_LOGD(minuet::TAG, "Turning off light");
  light->turn_off().perform();
}

void turn_on_with_default_color(esphome::light::LightState *light) {
  ESP_LOGD(minuet::TAG, "Turning on light with default color");
  make_call_with_white(light).set_state(true).set_brightness(1.f).perform();
}

void toggle(esphome::light::LightState *light) {
  if (light->remote_values.is_on()) {
    turn_off(light);
  } else {
    turn_on_with_default_color(light);
  }
}

void change_brightness(esphome::light::LightState *light, int direction) {
  if (light->remote_values.is_on()) {
    ESP_LOGD(minuet::TAG, "Changing light brightness by %d", direction);
    constexpr int BRIGHTNESS_LEVELS = 5;
    const int brightness = int(light->remote_values.get_brightness() * BRIGHTNESS_LEVELS);
    light->make_call()
        .set_brightness(float(std::max(std::min(brightness + direction, BRIGHTNESS_LEVELS), 1)) / BRIGHTNESS_LEVELS)
        .perform();
  }
}

const auto PRESET_MODES = std::initializer_list<Mode>{
    {.type = ModeType::WHITE},
    {.type = ModeType::COLOR, .wrgb = 0xff0000},  // Red
    {.type = ModeType::COLOR, .wrgb = 0xff00ff},  // Violet
    {.type = ModeType::EFFECT, .effect = "Rainbow"},
};

void cycle_preset_mode(esphome::light::LightState *light) {
  const auto &values = light->remote_values;
  if (values.is_on()) {
    FloatColor color;
    switch (values.get_color_mode()) {
      case esphome::light::ColorMode::RGB_WHITE:
        color = {
            .w = values.get_white(),
            .r = values.get_color_brightness() ? values.get_red() : 0,
            .g = values.get_color_brightness() ? values.get_green() : 0,
            .b = values.get_color_brightness() ? values.get_blue() : 0,
        };
        break;
      case esphome::light::ColorMode::RGB:
        color = {
            .w = 0.f,
            .r = values.get_red(),
            .g = values.get_green(),
            .b = values.get_blue(),
        };
        break;
      default:
      case esphome::light::ColorMode::WHITE:
      case esphome::light::ColorMode::BRIGHTNESS:
      case esphome::light::ColorMode::ON_OFF:
        color = {
            .w = 1.f,
        };
        break;
    }
    const bool has_effect = light->get_current_effect_index() != 0;
    const esphome::StringRef effect_name = light->get_effect_name();

    auto mode =
        std::find_if(PRESET_MODES.begin(), PRESET_MODES.end(), [color, has_effect, effect_name](const Mode &candidate) {
          switch (candidate.type) {
            case ModeType::WHITE:
              return !has_effect && color.w != 0 && color.r == 0 && color.g == 0 && color.b == 0;
            case ModeType::COLOR:
              return !has_effect && is_similar_color(color, to_float_color(candidate.wrgb));
            case ModeType::EFFECT:
              return has_effect && effect_name == candidate.effect;
            default:
              return false;
          }
        });
    if (mode == PRESET_MODES.end() || ++mode == PRESET_MODES.end()) {
      mode = PRESET_MODES.begin();
    }
    ESP_LOGD(minuet::TAG, "Applying light preset %u", mode - PRESET_MODES.begin());
    make_call_with_mode(light, *mode).perform();
  } else {
    turn_on_with_default_color(light);
  }
}

enum class Action : uint8_t {
  TURN_OFF,
  TURN_ON,
  BRIGHTNESS_UP,
  BRIGHTNESS_DOWN,
  SET_WHITE,
  SET_COLOR,
  SET_EFFECT,
};

struct CommandAction {
  uint8_t command;
  Action action;
  union {
    const char *effect;
    WRGBColor wrgb;
  };
};

// Table of actions for a common 24 key infrared light remote control.
const auto COMMAND_ACTIONS = std::initializer_list<CommandAction>{
    {.command = 0x00, .action = Action::BRIGHTNESS_UP},
    {.command = 0x01, .action = Action::BRIGHTNESS_DOWN},
    {.command = 0x02, .action = Action::TURN_OFF},
    {.command = 0x03, .action = Action::TURN_ON},
    {.command = 0x07, .action = Action::SET_WHITE},
    {.command = 0x04, .action = Action::SET_COLOR, .wrgb = 0x00ff0000},    // Red
    {.command = 0x05, .action = Action::SET_COLOR, .wrgb = 0x0000ff00},    // Green
    {.command = 0x06, .action = Action::SET_COLOR, .wrgb = 0x000000ff},    // Blue
    {.command = 0x08, .action = Action::SET_COLOR, .wrgb = 0x00d32f2f},    // Red 700
    {.command = 0x09, .action = Action::SET_COLOR, .wrgb = 0x00388e3c},    // Green 700
    {.command = 0x0a, .action = Action::SET_COLOR, .wrgb = 0x001976d2},    // Blue 700
    {.command = 0x0c, .action = Action::SET_COLOR, .wrgb = 0x00e64a19},    // Deep Orange 700
    {.command = 0x0d, .action = Action::SET_COLOR, .wrgb = 0x000097a7},    // Cyan 700
    {.command = 0x0e, .action = Action::SET_COLOR, .wrgb = 0x00512da8},    // Deep Purple 700
    {.command = 0x10, .action = Action::SET_COLOR, .wrgb = 0x00ffa000},    // Amber 700
    {.command = 0x11, .action = Action::SET_COLOR, .wrgb = 0x000288d1},    // Light Blue 700
    {.command = 0x12, .action = Action::SET_COLOR, .wrgb = 0x007b1fa2},    // Purple 700
    {.command = 0x14, .action = Action::SET_COLOR, .wrgb = 0x00fbc02d},    // Yellow 700
    {.command = 0x15, .action = Action::SET_COLOR, .wrgb = 0x00303f9f},    // Indigo 700
    {.command = 0x16, .action = Action::SET_COLOR, .wrgb = 0x00c2185b},    // Pink 700
    {.command = 0x0b, .action = Action::SET_EFFECT, .effect = "Twinkle"},  // Labeled "FLASH"
    {.command = 0x0f, .action = Action::SET_EFFECT, .effect = "Pulse"},    // Labeled "STROBE"
    {.command = 0x13, .action = Action::SET_EFFECT, .effect = "Fade"},     // Labeled "FADE"
    {.command = 0x17, .action = Action::SET_EFFECT, .effect = "Rainbow"},  // Labeled "SMOOTH"
};

void apply_nec_code(esphome::light::LightState *light, esphome::remote_base::NECData code) {
  const unsigned command = code.command & 0xff;
  if (code.address != 0xef00 || code.command_repeats != 1 || command != ((code.command >> 8) ^ 0xff)) {
    return;
  }
  const auto command_action = std::find_if(COMMAND_ACTIONS.begin(), COMMAND_ACTIONS.end(),
      [command](const CommandAction &item) { return item.command == command; });
  if (command_action == COMMAND_ACTIONS.end()) {
    ESP_LOGD(minuet::TAG, "Received unknown light remote control code %d", command);
    return;
  }

  const bool on = light->remote_values.is_on();
  const auto log_command = [](const char *name) { ESP_LOGI(minuet::TAG, "Command: light remote control %s", name); };
  switch (command_action->action) {
    case Action::TURN_ON:
      log_command("ON");
      if (!on) {
        turn_on_with_default_color(light);
      }
      break;
    case Action::TURN_OFF:
      log_command("OFF");
      if (on) {
        turn_off(light);
      }
      break;
    case Action::BRIGHTNESS_UP:
      log_command("BRIGHTNESS UP");
      if (on) {
        change_brightness(light, 1);
      }
      break;
    case Action::BRIGHTNESS_DOWN:
      log_command("BRIGHTNESS DOWN");
      if (on) {
        change_brightness(light, -1);
      }
      break;
    case Action::SET_WHITE:
      log_command("WHITE");
      if (on) {
        make_call_with_white(light).perform();
      }
      break;
    case Action::SET_COLOR:
      ESP_LOGI(minuet::TAG, "Command: light remote control COLOR %d, wrgb #%08lx", command, command_action->wrgb);
      if (on) {
        make_call_with_color(light, command_action->wrgb).perform();
      }
      break;
    case Action::SET_EFFECT:
      ESP_LOGI(minuet::TAG, "Command: light remote control EFFECT %d, name %s", command, command_action->effect);
      if (on) {
        make_call_with_effect(light, command_action->effect).perform();
      }
      break;
  }
}

}  // namespace light
}  // namespace accessory
}  // namespace minuet
