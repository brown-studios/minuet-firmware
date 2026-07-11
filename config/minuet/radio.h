// MINUET RADIO HEADER

#pragma once

#ifndef MINUET_RADIO
#error "Only include radio.h when MINUET_RADIO is defined"
#endif

namespace minuet {
namespace radio {

bool is_at_least_one_radio_on() {
#ifdef MINUET_RADIO_BLUETOOTH
  if (minuet_radio_bluetooth->state) {
    return true;
  }
#endif
#ifdef MINUET_RADIO_WIFI
  if (minuet_radio_wifi->state) {
    return true;
  }
#endif
  return false;
}

void control_radios_jointly(bool new_state) {
#ifdef MINUET_RADIO_BLUETOOTH
  minuet_radio_bluetooth->control(new_state);
#endif
#ifdef MINUET_RADIO_WIFI
  minuet_radio_wifi->control(new_state);
#endif
}

bool toggle_radios_jointly() {
  const bool new_state = !is_at_least_one_radio_on();
  control_radios_jointly(new_state);
  return new_state;
}

void turn_on_and_pair() {
  control_radios_jointly(true);
  minuet_radio_pairing_mode->turn_on();
}

}  // namespace radio
}  // namespace minuet
