// MINUET TONE HEADER

#pragma once

#include <string>

namespace minuet {
namespace tone {

void play(const char *tone) {
  minuet_tone->execute(tone);
}

}  // namespace tone
}  // namespace minuet
