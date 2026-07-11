// MINUET TONE HEADER

#pragma once

#include <string>

#include "esphome/core/alloc_helpers.h"
#include "esphome/core/string_ref.h"

namespace minuet {
namespace tone {

void play_rtttl(std::string rtttl) {
  const char severity = rtttl[0];
  switch (minuet_audible_feedback->active_index().value_or(0)) {
    case 0:  // Polite
      if (severity == '?')
        return;
      break;
    case 1:  // More
      break;
    case 2:  // Less
      if (severity != '!')
        return;
      break;
  }
  minuet_tone_generator->play(std::move(rtttl));
}

void play(const char *tone) {
  minuet_tone->execute(esphome::StringRef(tone));
}

void play_beep() {
  play("beep");
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
  play_rtttl(esphome::str_sprintf("?fan_speed:d=16,o=4,b=144:32%s,32%s", SPEED_NOTE[old_speed], SPEED_NOTE[new_speed])
          .c_str());
}

}  // namespace tone
}  // namespace minuet
