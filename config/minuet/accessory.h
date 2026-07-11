// MINUET ACCESSORY HEADER
//
// Maintains a registry of installed accessories.

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace accessory {

// Defines an accessory
struct Accessory {
  // The ID of the accessory.  It should match the YAML file that defines its behavior
  // and be distinct from the ID of any other accessory included in the configuration.
  // e.g. "light"
  const char *id{nullptr};

  // Set event handler lambdas for the events that the accessory handles (or nullptr if unhandled).
  using EventHandler = void (*)();
  using NECCodeEventHandler = void (*)(esphome::remote_base::NECData code);

  // Called when the "accessory toggle" action is performed on the keypad.
  // Key combination: hold direction.
  // e.g. Toggle the light on or off.
  EventHandler handle_toggle{nullptr};

  // Called when the "accessory up" action is performed on the keypad.
  // Key combination: press direction and up.
  // e.g. Turn the light on.
  EventHandler handle_up{nullptr};

  // Called when the "accessory down" action is performed on the keypad.
  // Key combination: press direction and down.
  // e.g. Turn the light off.
  EventHandler handle_down{nullptr};

  // Called when the "accessory mode" action is performed on the keypad.
  // Key combination: press direction and up and down (aka. direction and open/close).
  // e.g. Change the light color and activate special effects.
  EventHandler handle_mode{nullptr};

  // Called when the IR receiver receives an NEC code from a remote control.
  // e.g. Set the light to the brightness and color requested by the remote.
  NECCodeEventHandler handle_nec_code{nullptr};

  // Called when one or more of the safety lock conditions may have changed.
  // e.g. Turn off the light when inhibited by the safety lock.
  EventHandler handle_safety_lock{nullptr};
};

std::vector<Accessory> registry;

void install(Accessory accessory) {
  ESP_LOGI(minuet::TAG, "Installed accessory: %s", accessory.id);
  registry.push_back(std::move(accessory));
}

bool is_installed(const char *accessory_id) {
  for (const auto &accessory : registry) {
    if (strcmp(accessory.id, accessory_id) == 0)
      return true;
  }
  return false;
}

template<typename Handler, typename... Args> void dispatch_handler(Handler Accessory::*handler_member, Args... args) {
  for (const auto &accessory : registry) {
    const auto &handler = accessory.*handler_member;
    if (handler) {
      handler(std::forward<Args>(args)...);
    }
  }
}

}  // namespace accessory
}  // namespace minuet
