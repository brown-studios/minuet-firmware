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

void turn_off(esphome::light::LightState *light) {
  light->turn_off().perform();
}

esphome::light::LightCall make_call_with_white_color(esphome::light::LightState *light) {
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

esphome::light::LightCall make_call_with_rgb_color(esphome::light::LightState *light, float r, float g, float b) {
  auto call = light->make_call();
  if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB_WHITE)) {
    call.set_color_mode(esphome::light::ColorMode::RGB_WHITE).set_color_brightness(1.f).set_rgbw(r, g, b, 0.f);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::RGB)) {
    call.set_color_mode(esphome::light::ColorMode::RGB).set_rgb(r, g, b);
  } else if (light->get_traits().supports_color_mode(esphome::light::ColorMode::BRIGHTNESS)) {
    call.set_color_mode(esphome::light::ColorMode::BRIGHTNESS);
  }
  return call.set_effect("none");
}

void turn_on_with_default_color(esphome::light::LightState *light) {
  make_call_with_white_color(light).set_state(true).set_brightness(1.f).perform();
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
    constexpr int BRIGHTNESS_LEVELS = 5;
    const int brightness = int(light->remote_values.get_brightness() * BRIGHTNESS_LEVELS);
    light->make_call()
        .set_brightness(float(std::max(std::min(brightness + direction, BRIGHTNESS_LEVELS), 1)) / BRIGHTNESS_LEVELS)
        .perform();
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
    unsigned rgb;
  };
};

// Table of actions for a common 24 key infrared light remote control.
const auto COMMAND_ACTIONS = std::initializer_list<CommandAction>{
    {.command = 0x00, .action = Action::BRIGHTNESS_UP},
    {.command = 0x01, .action = Action::BRIGHTNESS_DOWN},
    {.command = 0x02, .action = Action::TURN_OFF},
    {.command = 0x03, .action = Action::TURN_ON},
    {.command = 0x07, .action = Action::SET_WHITE},
    {.command = 0x04, .action = Action::SET_COLOR, .rgb = 0xff0000},       // Red
    {.command = 0x05, .action = Action::SET_COLOR, .rgb = 0x00ff00},       // Green
    {.command = 0x06, .action = Action::SET_COLOR, .rgb = 0x0000ff},       // Blue
    {.command = 0x08, .action = Action::SET_COLOR, .rgb = 0xd32f2f},       // Red 700
    {.command = 0x09, .action = Action::SET_COLOR, .rgb = 0x388e3c},       // Green 700
    {.command = 0x0a, .action = Action::SET_COLOR, .rgb = 0x1976d2},       // Blue 700
    {.command = 0x0c, .action = Action::SET_COLOR, .rgb = 0xe64a19},       // Deep Orange 700
    {.command = 0x0d, .action = Action::SET_COLOR, .rgb = 0x0097a7},       // Cyan 700
    {.command = 0x0e, .action = Action::SET_COLOR, .rgb = 0x512da8},       // Deep Purple 700
    {.command = 0x10, .action = Action::SET_COLOR, .rgb = 0xffa000},       // Amber 700
    {.command = 0x11, .action = Action::SET_COLOR, .rgb = 0x0288d1},       // Light Blue 700
    {.command = 0x12, .action = Action::SET_COLOR, .rgb = 0x7b1fa2},       // Purple 700
    {.command = 0x14, .action = Action::SET_COLOR, .rgb = 0xfbc02d},       // Yellow 700
    {.command = 0x15, .action = Action::SET_COLOR, .rgb = 0x303f9f},       // Indigo 700
    {.command = 0x16, .action = Action::SET_COLOR, .rgb = 0xc2185b},       // Pink 700
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
    ESP_LOGD(minuet::TAG, "Received unknown light remote control code: %d", command);
    return;
  }

  const bool on = light->remote_values.is_on();
  const auto log_command = [](const char *name) {
    ESP_LOGD(minuet::TAG, "Received light remote control code: %s", name);
  };
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
        make_call_with_white_color(light).perform();
      }
      break;
    case Action::SET_COLOR:
      ESP_LOGD(minuet::TAG, "Received light remote control code: COLOR %d, rgb #%06x", command, command_action->rgb);
      if (on) {
        const float red = ((command_action->rgb >> 16) & 0xff) / 255.f;
        const float green = ((command_action->rgb >> 8) & 0xff) / 255.f;
        const float blue = (command_action->rgb & 0xff) / 255.f;
        make_call_with_rgb_color(light, red, green, blue).perform();
      }
      break;
    case Action::SET_EFFECT:
      ESP_LOGD(minuet::TAG, "Received light remote control code: EFFECT %d, name %s", command, command_action->effect);
      if (on) {
        light->make_call().set_effect(command_action->effect).perform();
      }
      break;
  }
}

}  // namespace light
}  // namespace accessory
}  // namespace minuet
