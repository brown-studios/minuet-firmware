// MINUET HARDWARE HEADER
//
// Identifies the board variant at runtime using data recorded in eFuses at the factory.
// Used to ensure compatibility across board revisions.

#pragma once

#include <cstdint>

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace hardware {

using Series = uint8_t;
using VersionMajor = uint8_t;
using VersionMinor = uint8_t;
using Patch = uint8_t;

constexpr Patch PATCH_NONE = 0;
constexpr Patch PATCH_A = 1 << 0;
constexpr Patch PATCH_B = 1 << 1;
constexpr Patch PATCH_C = 1 << 2;
constexpr Patch PATCH_D = 1 << 3;

struct BoardInfo {
  Series series;
  VersionMajor version_major;
  VersionMinor version_minor;
  Patch patch;
  bool assumed;
};
BoardInfo board_info;

unsigned read_board_info_field(uint8_t start_bit, uint8_t num_bits) {
  const esp_efuse_desc_t field = {EFUSE_BLK3, start_bit, num_bits};
  const esp_efuse_desc_t *field_list[] = {&field, nullptr};
  unsigned value = 0;
  esp_efuse_read_field_blob(field_list, &value, num_bits);
  return value;
}

void read_board_info() {
  board_info.series = read_board_info_field(0, 10);
  board_info.version_major = read_board_info_field(10, 5);
  board_info.version_minor = read_board_info_field(15, 5);
  board_info.patch |= read_board_info_field(20, 1) ? PATCH_A : 0;
  board_info.patch |= read_board_info_field(21, 1) ? PATCH_B : 0;
  board_info.patch |= read_board_info_field(22, 1) ? PATCH_C : 0;
  board_info.patch |= read_board_info_field(23, 1) ? PATCH_D : 0;

  if (!board_info.version_major && !board_info.version_minor) {
    ESP_LOGW(minuet::TAG,
        "Using an assumed board version compiled into the firmware which may result in compatibility issues; please "
        "set the board's eFuses according to the documentation");
    board_info.version_major = (MINUET_BOARD_ASSUMED_VERSION_MAJOR);
    board_info.version_minor = (MINUET_BOARD_ASSUMED_VERSION_MINOR);
    board_info.patch = (MINUET_BOARD_ASSUMED_PATCH_A ? PATCH_A : 0) | (MINUET_BOARD_ASSUMED_PATCH_B ? PATCH_B : 0) |
        (MINUET_BOARD_ASSUMED_PATCH_C ? PATCH_C : 0) | (MINUET_BOARD_ASSUMED_PATCH_D ? PATCH_D : 0);
    board_info.assumed = true;
  }

  char buf[100];
  snprintf(buf, sizeof(buf), "version %d.%d%s%s%s%s series %d%s", board_info.version_major, board_info.version_minor,
      (board_info.patch & PATCH_A) ? "A" : "", (board_info.patch & PATCH_B) ? "B" : "",
      (board_info.patch & PATCH_C) ? "C" : "", (board_info.patch & PATCH_D) ? "D" : "", board_info.series,
      board_info.assumed ? " (assumed)" : "");
  ESP_LOGI(minuet::TAG, "Board info: %s", buf);
  minuet_board_info->publish_state(buf);
}

bool is_before_version_and_patch(VersionMajor major, VersionMinor minor, Patch patch) {
  return board_info.version_major < major || (board_info.version_major == major && board_info.version_minor < minor) ||
      (board_info.version_major == major && board_info.version_minor == minor && (board_info.patch & patch) != patch);
}

}  // namespace hardware
}  // namespace minuet
