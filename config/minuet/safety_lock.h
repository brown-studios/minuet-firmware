// MINUET SAFETY LOCK HEADER
//
// Inhibits certain operations while the safety lock is active.

#pragma once

#include "accessory.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "tone.h"
#include "utility.h"

namespace minuet {
namespace safety_lock {

enum class Stage : uint8_t {
  SPECULATIVE = 0,
  ONGOING = 1,
  INITIATING = 2,
};

struct Operation {
  // A brief description of the operation to be performed.
  const char *description;
  // The stage of the operation determines latching behavior and alerts in case of inhibition.
  // - SPECULATIVE: No latch, no alerts.
  // - ONGOING: May latch, may write log messages.
  // - INITIATING: May latch, may write log messages and provide audible feedback.
  Stage stage;
  // Whether the operation should be inhibited by rain.
  bool rain_sensitive;
};

bool validate_operation(Operation operation) {
  bool inhibited = false;
  const char *tone = nullptr;
  if (operation.rain_sensitive) {
    if (operation.stage >= Stage::ONGOING && minuet_rain_safety_lock->state &&
        !minuet_rain_safety_lock_latched->state) {
      minuet_rain_safety_lock_latched->publish_state(true);
    }
    if (minuet_rain_safety_lock->state || minuet_rain_safety_lock_latched->state) {
      if (operation.stage >= Stage::ONGOING) {
        ESP_LOGI(minuet::TAG, "Rain safety lock inhibited '%s': reset the rain sensor to clear the lock",
            operation.description);
      }
      if (!inhibited) {
        inhibited = true;
        tone = "forbidden_by_rain_lock";
      }
    }
  }
  if (minuet_battery_safety_lock->state) {
    if (operation.stage >= Stage::ONGOING) {
      ESP_LOGI(minuet::TAG,
          "Battery safety lock inhibited '%s': check the battery voltage (actual %0.3f V, low "
          "limit %0.3f V, high limit %0.3f V)",
          operation.description, minuet_battery_voltage->state, minuet_battery_voltage_low->state,
          minuet_battery_voltage_high->state);
    }
    if (!inhibited) {
      inhibited = true;
      tone = "forbidden_by_battery_lock";
    }
  }
  if (minuet_accessory_safety_lock->state) {
    if (operation.stage >= Stage::ONGOING) {
      ESP_LOGI(minuet::TAG,
          "Accessory safety lock inhibited '%s': check installed accessories (such as the cover "
          "sensor) to clear the lock",
          operation.description);
    }
    if (!inhibited) {
      inhibited = true;
      tone = "forbidden_by_accessory_lock";
    }
  }
  if (minuet_manual_safety_lock->state) {
    if (operation.stage >= Stage::ONGOING) {
      ESP_LOGI(minuet::TAG, "Manual safety lock inhibited '%s': use the keypad or an app to clear the lock",
          operation.description);
    }
    if (!inhibited) {
      inhibited = true;
      tone = "forbidden_by_manual_lock";
    }
  }
  if (minuet_automation_safety_lock->state) {
    if (operation.stage >= Stage::ONGOING) {
      ESP_LOGI(minuet::TAG, "Automation safety lock inhibited '%s': use an app to clear to clear the lock",
          operation.description);
    }
    if (!inhibited) {
      inhibited = true;
      tone = "forbidden_by_automation_lock";
    }
  }
  if (inhibited && operation.stage >= Stage::INITIATING) {
    minuet::tone::play(tone);
  }
  return !inhibited;
}

using UpdateCallback = void();
esphome::CallbackManager<UpdateCallback> on_update;

void schedule_update() {
  esphome::App.scheduler.set_timeout(nullptr, "safety lock update", 0, [] {
    on_update.call();
    minuet::accessory::dispatch_handler(&minuet::accessory::Accessory::handle_safety_lock_update);
  });
}

}  // namespace safety_lock
}  // namespace minuet
