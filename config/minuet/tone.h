// MINUET TONE HEADER

#pragma once

#include <string>

#include "esphome/core/alloc_helpers.h"

namespace minuet {
namespace tone {

void play(const char *tone) {
  minuet_tone->execute(tone);
}

constexpr const char *SPEED_NOTE[11] = {
    "p",
    "e6",
    "f6",
    "f#6",
    "g6",
    "g#6",
    "a7",
    "a#7",
    "b7",
    "c7",
    "c#7",
};

void play_fan_speed(int old_speed, int new_speed) {
  minuet_tone_rtttl->execute(
      esphome::str_sprintf("?fan_speed:d=16,o=4,b=144:32%s,32%s", SPEED_NOTE[old_speed], SPEED_NOTE[new_speed])
          .c_str());
}

}  // namespace tone
}  // namespace minuet
