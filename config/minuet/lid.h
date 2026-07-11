// MINUET LID HEADER
//
// Interfaces with the DRV8876 motor driver for the lid.

#pragma once

#include <cmath>
#include <cstdint>

#include "utility.h"

namespace minuet {
namespace lid {

bool is_open_or_opening() {
  return (minuet_lid->current_operation == esphome::cover::COVER_OPERATION_IDLE &&
             minuet_lid->position == esphome::cover::COVER_OPEN) ||
      minuet_lid->current_operation == esphome::cover::COVER_OPERATION_OPENING;
}

enum class MotorStatus : uint8_t {
  NOT_READY = 0,
  OPEN,
  CLOSE,
  STOP,
  FAULT,
  STALL,
  TIMEOUT,
};

// Controls the DRV8876 motor driver chip.
class MotorController {
 public:
  void init();
  void shutdown();
  void start(bool open, float duty_initial, float duty_final, float duty_ramp_secs, float timeout_secs);
  void stop();
  bool poll();

  MotorStatus status() const {
    return this->status_;
  }
  const char *format_status() const;

 private:
  void sleep_();

  MotorStatus status_{MotorStatus::NOT_READY};
  float duty_initial_{};
  float duty_final_{};
  uint32_t duty_ramp_ms_{};
  uint32_t timeout_ms_{};
  uint32_t start_time_{};
};

void MotorController::init() {
  if (this->status_ != MotorStatus::NOT_READY) {
    return;
  }

  ESP_LOGI(minuet::TAG, "Initializing the lid motor driver");
  this->sleep_();
  this->status_ = MotorStatus::STOP;
}

void MotorController::shutdown() {
  if (this->status_ == MotorStatus::NOT_READY) {
    return;
  }

  ESP_LOGI(minuet::TAG, "Shutting down the lid motor driver");
  this->sleep_();
  this->status_ = MotorStatus::NOT_READY;
}

void MotorController::start(bool open, float duty_initial, float duty_final, float duty_ramp_secs, float timeout_secs) {
  ESP_LOGI(minuet::TAG,
      "Start lid motor: open=%d, duty_initial=%.3f, duty_final=%.3f, duty_ramp_secs=%.3f, timeout_secs=%.3f", open,
      duty_initial, duty_final, duty_ramp_secs, timeout_secs);
  if (this->status_ == MotorStatus::NOT_READY) {
    ESP_LOGW(minuet::TAG, "Lid motor driver not ready");
    return;
  }

  this->status_ = open ? MotorStatus::OPEN : MotorStatus::CLOSE;
  this->duty_initial_ = duty_initial;
  this->duty_final_ = duty_final;
  this->duty_ramp_ms_ = duty_ramp_secs * 1000;
  this->timeout_ms_ = timeout_secs * 1000;
  this->start_time_ = esphome::millis();

  // Begin the movement in the motor brake state, wait for poll to update the duty cycle
  minuet_lid_motor_in1->set_level(1);
  minuet_lid_motor_in2->set_level(1);
  minuet_lid_motor_sleep->set_state(false);
}

void MotorController::stop() {
  ESP_LOGI(minuet::TAG, "Stop lid motor");
  if (this->status_ != MotorStatus::OPEN && this->status_ != MotorStatus::CLOSE) {
    return;
  }

  this->sleep_();
  this->status_ = MotorStatus::STOP;
}

bool MotorController::poll() {
  const bool fault = minuet_lid_motor_fault->state;
  const bool stall = minuet_lid_motor_stall->state;
  if (this->status_ != MotorStatus::OPEN && this->status_ != MotorStatus::CLOSE) {
    if (fault || stall) {
      ESP_LOGD(minuet::TAG, "Waiting for lid motor conditions to clear: fault=%d, stall=%d", fault, stall);
      return true;  // wait for the conditions to clear
    }
    return false;  // done
  }

  if (fault) {
    this->sleep_();
    this->status_ = MotorStatus::FAULT;
    ESP_LOGW(minuet::TAG, "Lid motor stopped due to fault");
    return true;  // wait for the fault to clear
  }

  if (stall) {
    this->sleep_();
    this->status_ = MotorStatus::STALL;
    ESP_LOGI(minuet::TAG, "Lid motor stopped at end of travel");
    return true;  // wait for the stall to clear
  }

  uint32_t runtime_ms = esphome::millis() - this->start_time_;
  if (runtime_ms >= this->timeout_ms_) {
    this->sleep_();
    this->status_ = MotorStatus::TIMEOUT;
    ESP_LOGW(minuet::TAG, "Lid motor stopped due to timeout");
    return false;  // abort due to timeout
  }

  float level = runtime_ms >= this->duty_ramp_ms_
      ? this->duty_final_
      : std::lerp(this->duty_initial_, this->duty_final_, float(runtime_ms) / this->duty_ramp_ms_);
  ESP_LOGVV(minuet::TAG, "Lid motor duty cycle: %.3f", level);
  (this->status_ == MotorStatus::CLOSE ? minuet_lid_motor_in1 : minuet_lid_motor_in2)->set_level(1.0f - level);
  return true;  // keep running the motor
}

void MotorController::sleep_() {
  minuet_lid_motor_sleep->set_state(true);
  minuet_lid_motor_in1->set_level(0);
  minuet_lid_motor_in2->set_level(0);
}

const char *MotorController::format_status() const {
  constexpr const char *MOTOR_STATUS_TEXT[] = {
      "NOT_READY",
      "OPEN",
      "CLOSE",
      "STOP",
      "FAULT",
      "STALL",
      "TIMEOUT",
  };
  return MOTOR_STATUS_TEXT[int(this->status_)];
}

MotorController motor_controller{};

}  // namespace lid
}  // namespace minuet
