// MINUET FAN HEADER
//
// Interfaces with the MCF8316D BLDC motor driver for the fan.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "esphome/components/fan/fan.h"
#include "esphome/components/mcf8316/mcf8316.h"
#include "esphome/core/log.h"
#include "utility.h"

namespace minuet {
namespace fan {

using namespace esphome::mcf8316;
using ErrorCode = MCF8316Component::ErrorCode;

bool is_on() {
  return minuet_fan->state;
}

constexpr bool direction_is_exhaust(esphome::fan::FanDirection direction) {
  return direction == esphome::fan::FanDirection::REVERSE;
}

constexpr esphome::fan::FanDirection make_direction(bool exhaust) {
  return exhaust ? esphome::fan::FanDirection::REVERSE : esphome::fan::FanDirection::FORWARD;
}

constexpr float rpm_to_hz(float rpm) {
  return rpm / 60.f;
}

constexpr float hz_to_rpm(float hz) {
  return hz * 60.f;
}

// Set to true after the hardware components for the fan have been setup.
// Attempts to initialize or update the fan state are suppressed until this is true.
bool hardware_setup_complete{};

// Describes a brushless DC fan motor and the parameters needed to operate it.
// Refer to the MCF8316D datasheet for more information.
struct MotorProfile final {
  // FG divisor, number of motor pole pairs.
  //
  // Choose this value by consulting the motor datasheet.
  unsigned fg_div : 4 {0};

  // Motor BEMF lead angle.
  //
  // Can be optimized for drive efficiency at higher speeds but 0 is fine.
  unsigned lead_angle : 5 {0};

  // Motor phase resistance (MPET).
  unsigned motor_res : 8 {0};

  // Motor phase inductance (MPET).
  unsigned motor_ind : 8 {0};

  // Motor BEMF constant (MPET).
  unsigned motor_bemf_const : 8 {0};

  // Speed loop Kp coefficient (MPET).
  unsigned spd_loop_kp : 10 {0};

  // Speed loop Ki coefficient (MPET).
  unsigned spd_loop_ki : 10 {0};

  // IPD clock frequency.
  //
  // Increasing this value reduces the "tick" heard during IPD motor startup.
  // Too high may cause IPD startup faults.
  IPDClockFrequency ipd_clk_freq : 3 {0};

  // IPD current threshold.
  //
  // Reducing this value reduces the "tick" heard during IPD motor startup.
  // Too low may cause IPD startup to incorrectly determing the motor alignment and
  // cause the motor to start spinning backwards.
  IPDCurrentThreshold ipd_curr_thr : 5 {0};

  // Open loop current limit per phase.
  //
  // Choose this value to provide enough current for the motor to startup and run
  // up to `OPN_CL_HANDOFF_THR` speed.  Too low and the motor may stall before reaching
  // this speed which will cause an MPET fault.  Should be about 1/2 to 2/3 of `ILIMIT`.
  CurrentLimit ol_ilimit : 4 {0};

  // Open loop acceleration.
  //
  // Choose this value to ensure a smooth handoff from the IPD advance into open loop
  // control.  Too low and the motor may stall on startup.  Too high and the motor
  // may lose synchronization with then open loop if it cannot speed up fast enough to
  // match the acceleration due to rotor inertia.
  //
  // Note: The acceleration rate is defined in electrical Hz per second (not rotor Hz per second)
  // so the appropriate value to use depends on the number of motor poles and inertia.
  OpenLoopAcceleration ol_acc_a1 : 4 {0};

  // Closed loop current limit per phase.
  //
  // Choose this value by consulting the motor datasheet.
  // Must be less than BOARD_LOCK_ILIMIT.
  CurrentLimit ilimit : 4 {0};  // Closed loop current limit per phase

  // Closed loop acceleration when not fully aligned.
  //
  // Should be equal to or a little greater than OL_ACC_A1.  Accelerating too quickly
  // while the closed loop is not fully aligned may cause loss of motor synchronization.
  // It's fine to keep accelerating at a similar pace to the open loop.
  //
  // Note: The acceleration rate is defined in electrical Hz per second (not rotor Hz per second)
  // so the appropriate value to use depends on the number of motor poles and inertia.
  ClosedLoopSlowAcceleration cl_slow_acc : 4 {0};

  // Closed loop acceleration when fully aligned.
  //
  // Should be much greater than CL_SLOW_ACC so the motor quickly comes up to speed.
  // The acceleration rate is defined in electrical Hz per second (not rotor Hz per second)
  // so the appropriate value to use depends on the number of motor poles.
  //
  // Note: The acceleration rate is defined in electrical Hz per second (not rotor Hz per second)
  // so the appropriate value to use depends on the number of motor poles and inertia.
  ClosedLoopAcceleration cl_acc : 5 {0};

  // Closed loop deceleration.
  //
  // Deceleration is generally limited by rotor inertia so this value can be set arbitrarily large
  // or be unlimited with no significant effect.
  //
  // Note: The acceleration rate is defined in electrical Hz per second (not rotor Hz per second)
  // so the appropriate value to use depends on the number of motor poles and inertia.
  ClosedLoopDeceleration cl_dec : 5 {0};

  // The slowest rotational speed that the motor can sustain under load (with the fan blades attached)
  // quietly and without stalling.
  uint16_t fan_speed_rpm_min{0};

  // The fastest rotational speed that the motor can sustain under load (with the fan blades attached)
  // safely, within the configured current limit, and without excess vibration.
  uint16_t fan_speed_rpm_max{0};
} __attribute__((packed));

// Describes a motor's provenance and parameters.
struct MotorDescriptor final {
  const char *label;
  MotorProfile profile;
};

// This constant determines the 100% reference input speed for the motor driver.
// It is converted to electrical Hz and assigned to the MAX_SPEED parameter.
//
// Many of the MCF8316D's configuration parameters are defined as a percentage
// of MAX_SPEED so don't change the value of this constant unless you intend to
// change all of those parameters too.  This value is already set above the
// maximum speed that Minuet will command the fan motor to achieve.
//
// Please don't change this constant arbitrarily.  To set the motor's minimum
// and maximum speed, use the motor profile speed_rpm_min and speed_rpm_max fields.
constexpr float BOARD_MAX_SPEED_RPM = 2000.f;

// This constant limits the maximum power that the MCF8316D can draw from the DC bus.
// The maximum power that can be safely used is ultimately limited by the thermal
// capacity of the Minuet PCB and its power supply.  We assume that Minuet is
// powered from a large battery that can supply much more power than this limit.
//
// The Minuet PCB traces are rated for at least 4 A, the supply is protected
// with a 4 A polyfuse and has a nominal voltage of 12 V for a nominal maximum power
// draw of 48 W for the board.  So we set the limit to somewhat less to ensure a margin.
//
// The DC bus power limit should never be exceeded in normal operation even when the
// motor is running at fully speed under load.  Most of the inductive energy circulates
// between the motor phases and the MCF8316D so the phase current limits are more
// constraining than the bus power limit.
constexpr float BOARD_DC_BUS_MAX_POWER_WATTS = 40.f;

// This constant sets the motor phase current limit beyond which the MCF8316D reports
// a software-detected overcurrent lock fault and stops the motor.  The motor profile's
// ILIMIT must be smaller than this value to prevent spurious faults.
//
// The Minuet hardware design target for maximum motor phase peak current is 4 A which
// is enough current to achieve 1200 RPM with the kit motor and seems to work well.
// Operating motors with more current produces diminishing returns and risks triggering
// overtemperature faults.
//
// This value should be validated experimentally to ensure safe operation.
constexpr CurrentLimit BOARD_LOCK_ILIMIT = CurrentLimit::LIMIT_5_0_A;

// This constant sets the motor phase current limit beyond which the MCF8316D reports
// a hardware-detected overcurrent lock fault and stops the motor.  It behaves similarly
// to BOARD_LOCK_ILIMIT but it acts much more quickly (like a circuit breaker) for brief
// current spikes as seen during motor braking (which is essentially a short circuit).
//
// This value should be a little greater than BOARD_LOCK_ILIMIT to prevent spurious faults.
constexpr CurrentLimit BOARD_HW_LOCK_ILIMIT = CurrentLimit::LIMIT_5_5_A;

// Controls the MCF8316D motor driver chip.
class MotorController {
 public:
  static inline MCF8316Component *driver() {
    return minuet_fan_motor_driver;
  }

  void init(const MotorDescriptor &descriptor);
  void shutdown();

  bool set_state(float speed_rpm, bool exhaust, bool brake, bool keep_awake);
  void start_mpet();

  float get_tachometer_rpm();
  float get_bus_current();
  float get_motor_phase_peak_current();
  float get_vm_voltage();
  float get_fan_speed_rpm_for_index(int index) const;

  bool is_ready() const {
    return this->ready_;
  }
  std::string format_status() const;

  bool is_algorithm_running() const {
    AlgorithmState state = driver()->algorithm_state();
    return state != AlgorithmState::MOTOR_IDLE && state != AlgorithmState::MOTOR_FAULT;
  }

  bool inputs_active() const {
    return this->inputs_active_;
  }

  bool is_awake() const {
    return driver()->is_awake();
  }

 private:
  Config make_config_(const MotorProfile &profile);
  bool set_inputs_(float speed_in_rotor_hz, bool exhaust, bool brake);
  void clear_inputs_and_sleep_();

  bool ready_{};
  MotorProfile profile_{};
  bool inputs_active_{};
};

Config MotorController::make_config_(const MotorProfile &profile) {
  Config config = driver()->make_default_config();

  // Motor physical parameters
  config.set(FG_DIV, profile.fg_div);                      // Conversion factor from electrical Hz to rotor Hz
  config.set(LEAD_ANGLE, profile.lead_angle);              // BEMF lead angle
  config.set(MOTOR_RES, profile.motor_res);                // Motor phase resistance
  config.set(MOTOR_IND, profile.motor_ind);                // Motor phase inductance
  config.set(MOTOR_BEMF_CONST, profile.motor_bemf_const);  // Motor BEMF constant
  config.set(SPD_LOOP_KP, profile.spd_loop_kp);            // Speed loop Kp coefficient
  config.set(SPD_LOOP_KI, profile.spd_loop_ki);            // Speed loop Ki coefficient
  // N/A: CURR_LOOP_KP: only for current control loop
  // N/A: CURR_LOOP_KI: only for current control loop

  // Speed input
  config.set(MAX_SPEED,
      unsigned(convert_speed_in_rotor_hz_to_electrical_hz(rpm_to_hz(BOARD_MAX_SPEED_RPM), profile.fg_div) *
          6));  // Maximum speed with a 100% reference input
  config.set(
      INPUT_REFERENCE_WINDOW, InputReferenceWindow::WINDOW_DISABLED);  // Input reference window stops motor when speed
                                                                       // reference inside the window: disabled
  // N/A: SPEED_RANGE_SEL: only for PWM speed input
  // N/A: INPUT_MAXIMUM_FREQ: only for frequency speed input
  // N/A: SPEED_LIMIT_ENABLE: only for reference inputs other than speed

  // Reference profiles (speed input transfer function)
  config.set(REF_PROFILE_CONFIG, ReferenceProfileConfig::EQUATION);  // Reference profile configuration: equation, use
                                                                     // the speed input directly as-is
  config.set(DUTY_CLAMP1,
      0u);  // Duty cycle clamp 1: 0%, must be 0, otherwise would impose a minimum duty cycle on the speed input
  config.set(
      DUTY_HYS, 0u);  // Duty cycle hysteresis: 0%, must be 0, otherwise would impose hysteresis on the speed input
  config.set(MIN_DUTY, 0u);  // Minimum duty cycle: 1%, must be as small as possible, will not operate the motor when
                             // the speed input is below this threshold
  config.set(
      REF_CLAMP1, 0u);  // Reference clamp 1: 0%, must be 0, otherwise would impose a minimum value on the speed input
  // N/A: DUTY_ON1, DUTY_OFF1, DUTY_ON2, DUTY_OFF2, DUTY_CLAMP2
  // N/A: DUTY_A, DUTY_B, DUTY_C, DUTY_D, DUTY_E
  // N/A: REF_ON1, REF_OFF1, REF_ON2, REF_OFF2, REF_CLAMP2
  // N/A: REF_A, REF_B, REF_C, REF_D, REF_E
  // N/A: VOLTAGE_MODE_CONFIG, DUTY_COMMAND_FILTER

  // Initial speed detection
  config.set(ISD_EN, true);    // Initial speed detection: enabled
  config.set(BRAKE_EN, true);  // ISD brake state: enabled
  config.set(HIZ_EN, false);   // ISD HI-Z state: disabled, coasting is not needed
  // N/A: HIZ_TIME: only for ISD HI-Z state
  config.set(RESYNC_EN, true);  // ISD resynchronization state: enabled
  config.set(
      FW_DRV_RESYN_THR, ForwardDriveResyncThreshold::THRESHOLD_20_PERCENT);  // Resync speed threshold: 20% of MAX_SPEED
  config.set(STAT_DETECT_THR,
      StationaryDetectionThreshold::THRESHOLD_50_MV);  // Stationary detection BEMF threshold: 50 mV, set to a low BEMF
                                                       // to ensure rotor really has stopped, particularly when changing
                                                       // directions
  config.set(
      ISD_BEMF_FILT_ENABLE, true);  // Enable BEMF filter during ISD: enabled, recommended according to tuning guide
  config.set(
      ISD_STOP_TIME, ISDStopTime::TIME_50_MS);  // ISD detect motor stopped: after 50 ms, increase for better confidence
  config.set(
      ISD_RUN_TIME, ISDRunTime::TIME_50_MS);  // ISD detect motor running: after 50 ms, increase for better confidence
  config.set(ISD_TIMEOUT, ISDTimeout::TIME_1000_MS);  // ISD timeout: after 1000 ms
  config.set(FAST_ISD_EN, false);                     // Fast ISD: disabled, slow ISD is more reliable
  config.set(BRK_MODE, BrakeMode::BRAKE_LOW_SIDE);    // ISD brake mode: low-side brake
  config.set(
      BRK_CONFIG, BrakeConfig::BRAKE_CURRENT_AND_TIME);  // ISD brake config: exit brake state based on current and time
  config.set(BRK_CURR_THR, BrakeCurrentThreshold::BRAKE_CURRENT_0_1_A);  // ISD brake current threshold: 0.1 A
  config.set(BRK_TIME, BrakeTime::BRAKE_TIME_5000_MS);  // ISD brake stop time: 5000 ms, needs to be long enough to
                                                        // account for rotor inertia
  config.set(BRAKE_CURRENT_PERSIST,
      BrakeCurrentPersistence::TIME_500_MS);  // ISD brake current persistence: 500 ms, ensure enough time to confirm
                                              // that current below threshold means the motor really has stopped due to
                                              // rotor inertia

  // Motor startup with initial position detection
  config.set(MTR_STARTUP, MotorStartup::IPD);  // Motor startup: use IPD to avoid spinning the motor backwards, makes a
                                               // little "tick" sound during startup
  config.set(IPD_CLK_FREQ, profile.ipd_clk_freq);  // IPD clock frequency
  config.set(IPD_CURR_THR, profile.ipd_curr_thr);  // IPD current threshold
  config.set(
      IPD_RLS_MODE, IPDReleaseMode::TRISTATE);  // IPD release mode: tristate, faster decay than braking, less noisy
  config.set(IPD_ADV_ANGLE, IPDAdvanceAngle::ANGLE_30_DEG);  // IPD advance angle: 30 degrees
  config.set(IPD_REPEAT,
      IPDRepeat::REPEAT_3);  // IPD repeat count: 3 times, more reliable (2 times is enough but sometimes fails)
  config.set(IPD_HIGH_RESOLUTION_EN, true);  // IPD high resolution mode: enabled
  config.set(IPD_TIMEOUT_FAULT_EN, true);    // IPD timeout fault mode: enabled
  config.set(IPD_FREQ_FAULT_EN, true);       // IPD frequency fault mode: enabled
  // N/A: ALIGN_ANGLE: only for align startup mode
  // N/A: ALIGN_TIME: only for align startup mode
  // N/A: ALIGN_OR_SLOW_CURRENT_ILIMIT, only for align or slow current startup mode

  // Open loop control
  config.set(OL_ILIMIT, profile.ol_ilimit);                       // Open loop motor phase current limit
  config.set(OL_ACC_A1, profile.ol_acc_a1);                       // Open loop acceleration rate
  config.set(OL_ACC_A2, OpenLoopAcceleration2::ACCEL_0_0_HZ_S2);  // Open loop acceleration rate: 0 Hz/s^2, linear ramp
                                                                  // works well for a fan
  config.set(
      FIRST_CYCLE_FREQ_SEL, FirstCycleFrequencySelect::FREQ_0_HZ);  // First cycle frequency select: start from 0 Hz
  // SLOW_FIRST_CYC_FREQ: sets start frequency only when FIRST_CYCLE_FREQ_SEL is 1
  config.set(ALIGN_SLOW_RAMP_RATE, AlignSlowRampRate::LIMIT_50_A_S);  // Open loop current ramp rate: 50 A/s (used for
                                                                      // all startup modes, not just align)

  // Open loop to closed loop handoff
  config.set(
      AUTO_HANDOFF_EN, true);  // Auto handoff to closed loop: enabled, ignores OPN_CL_HANDOFF_THR except for MPET
  config.set(AUTO_HANDOFF_MIN_BEMF, AutoHandoffMinBEMF::BEMF_100_MV);  // Minimum BEMF for auto handoff: 100 mV
  config.set(OPN_CL_HANDOFF_THR,
      OpenToCloseLoopHandoffThreshold::THRESHOLD_25_PERCENT);  // Open to closed loop handoff speed: 25% of MAX_SPEED,
                                                               // used in MPET to get the motor running, ignored in
                                                               // normal operation due to auto handoff
  config.set(IQ_RAMP_EN,
      false);  // IQ ramp down in transition from open to closed loop: disabled, otherwise the transition takes too long
  config.set(THETA_ERROR_RAMP_RATE, ThetaErrorRampRate::RATE_0_10_DEG_MS);  // Theta error ramp: 0.1 deg/ms

  // Closed loop control
  config.set(ILIMIT, profile.ilimit);            // Closed loop motor phase current limit
  config.set(OVERMODULATION_ENABLE, false);      // Closed loop overmodulation: disabled to minimize acoustic noise
  config.set(CL_SLOW_ACC, profile.cl_slow_acc);  // Closed loop acceleration when not fully aligned
  config.set(CL_ACC, profile.cl_acc);            // Closed loop acceleration
  config.set(CL_DEC, profile.cl_dec);            // Closed loop deceleration

  // Braking initiated when speed input set to zero
  config.set(MTR_STOP, MotorStop::LOW_SIDE_BRAKE);  // Motor stop mode and closed loop brake behavior: low-side brake,
                                                    // performs a soft stop then brakes to a full stop before returning
                                                    // to the idle state (unlike HI_Z or RECIRCULATE)
  config.set(MTR_STOP_BRK_TIME, MotorStopBrakeTime::TIME_5000_MS);  // Motor stop time: 5000 ms, needs to be long enough
                                                                    // to account for rotor inertia
  config.set(LOW_SPEED_RECIRC_BRAKE_EN, false);  // Open loop brake behavior: recirculation disabled, only relevant if
                                                 // MTR_STOP is RECIRCULATE and we want low-side braking instead
  // N/A: ACT_SPIN_THR: only for MTR_STOP active spin-down mode

  // Braking initiated via brake pin
  config.set(BRAKE_SPEED_THRESHOLD,
      BrakeSpeedThreshold::THR_25_PERCENT);  // Brake speed threshold: reduce speed to 25% of MAX_SPEED before entering
                                             // brake state to prevent current limit lock fault
  config.set(BRAKE_PIN_MODE, BrakePinMode::LOW_SIDE_BRAKE);  // Brake pin mode: low-side brake
  // N/A: ALIGN_BRAKE_ANGLE_SEL: only used when BRAKE_PIN_MODE is align brake mode

  // Active braking
  config.set(ACTIVE_BRAKE_EN,
      false);  // Active braking: disabled, unnecessary, can cause voltage spikes during braking, requires extra tuning
  // N/A: ACTIVE_BRAKE_CURRENT_LIMIT
  // N/A: ACTIVE_BRAKE_BUS_CURRENT_SLEW_RATE
  // N/A: ACTIVE_BRAKE_KP
  // N/A: ACTIVE_BRAKE_KI
  // N/A: ACTIVE_BRAKE_SPEED_DELTA_LIMIT_ENTRY
  // N/A: ACTIVE_BRAKE_SPEED_DELTA_LIMIT_EXIT
  // N/A: ACTIVE_BRAKE_MOD_INDEX_LIMIT

  // Reverse drive
  config.set(DIR_CHANGE_MODE,
      DirChangeMode::STOP);      // Direction change mode: brake then change directions (don't reverse drive)
  config.set(RVS_DR_EN, false);  // Reverse drive: disabled, unnecessary and requires tuning
  // N/A: REV_DRV_HANDOFF_THR
  // N/A: REV_DRV_OPEN_LOOP_CURRENT
  // N/A: REV_DRV_OPEN_LOOP_ACCEL_A1
  // N/A: REV_DRV_OPEN_LOOP_ACCEL_A2
  // N/A: REV_DRV_OPEN_LOOP_DEC
  // N/A: REV_DRV_CONFIG

  // Flux weakening
  config.set(
      FLUX_WEAK_ENABLE, false);  // Flux weakining: disabled, we don't need to opeerate the motor above its rated speed
  // N/A: FLUX_WEAK_REF
  // N/A: FLUX_WEAK_KP
  // N/A: FLUX_WEAK_KI

  // Automatic lock retry
  config.set(LCK_RETRY, LockRetryTime::TIME_1000_MS);  // Retry interval after lock: 1 s
  config.set(
      AUTO_RETRY_TIMES, AutoRetryTimes::TIMES_3);  // Auto-retry number of times: 3 (not including the first attempt)

  // Software lock to protect PCB from overcurrent
  config.set(LOCK_ILIMIT, BOARD_LOCK_ILIMIT);                          // Software lock current limit
  config.set(LOCK_ILIMIT_DEG, LockCurrentLimitDeglitch::TIME_0_2_MS);  // Software lock deglitch time: 0.2 ms
  config.set(
      LOCK_ILIMIT_MODE, LockMode::AUTO_RETRY_HI_Z);  // Software lock mode: retry up to retry limit then latch fault

  // Hardware lock to protect PCB from overcurrent
  config.set(HW_LOCK_ILIMIT, BOARD_HW_LOCK_ILIMIT);                       // Hardware lock current limit
  config.set(HW_LOCK_ILIMIT_DEG, HWLockCurrentLimitDeglitch::TIME_2_US);  // Hardware lock deglitch time: 2 us
  config.set(
      HW_LOCK_ILIMIT_MODE, LockMode::AUTO_RETRY_HI_Z);  // Hardware lock mode: retry up to retry limit then latch fault

  // Motor lock to detect strange behavior of the motor such as when the rotor cannot spin
  config.set(LOCK1_EN, true);  // Motor lock on abnormal speed: enabled
  config.set(LOCK2_EN, true);  // Motor lock on abnormal BEMF: enabled
  config.set(LOCK3_EN, true);  // Motor lock on no motor: enabled
  config.set(LOCK_ABN_SPEED,
      AbnormalSpeedThreshold::THR_130_PERCENT);  // Motor lock abnormal speed threshold: 130% of max speed
  config.set(ABNORMAL_BEMF_THR, AbnormalBEMFThreshold::THR_65_PERCENT);  // Motor abnormal BEMF threshold: 65%
  config.set(ABNORMAL_BEMF_PERSISTENT_TIME,
      AbnormalBEMFPersistentTime::TIME_500_MS);                   // Motor abnormal BEMF detection time: 500 ms
  config.set(NO_MTR_THR, NoMotorCurrentThreshold::THR_0_0375_A);  // No motor current threshold: 0.0375 A
  config.set(NO_MTR_FLT_CLOSEDLOOP_DIS, false);                   // No motor fault detection in closed loop: enabled
  config.set(MTR_LCK_MODE, LockMode::AUTO_RETRY_HI_Z);  // Motor lock mode: retry up to retry limit then latch fault

  // Gate driver
  config.set(CIRCULAR_CURRENT_LIMIT_ENABLE, true);  // Circular current limit: `ILIMIT` sets peak phase current limit
  config.set(AVS_EN,
      true);  // Anti-voltage surge protection: enabled, prevent excess inductive energy from flowing back to the bus
  config.set(DEADTIME_COMP_EN, true);                // Deadtime compensation: enabled, reduce audible noise
  config.set(SLEW_RATE, SlewRate::SLEW_200_V_US);    // Slew rate: 200 V/us, factory default
  config.set(MIN_ON_TIME, MinOnTime::TIME_0_50_US);  // Low-side MOSFET minimum on-time: 0.5 us, factory default
  config.set(DYNAMIC_CSA_GAIN_EN, true);             // Dynamic current sense amplifier gain: enabled, factory default
  config.set(DYNAMIC_VOLTAGE_GAIN_EN, false);        // Dynamic voltage gain: disabled, factory default
  // N/A: CSA_GAIN: only for when DYNAMIC_CSA_GAIN_EN is disabled

  // Modulation
  config.set(SPREAD_SPECTRUM_MODULATION_DIS, false);              // Spread spectrum modulation: enabled
  config.set(PWM_MODE, PWMMode::CONTINUOUS);                      // PWM mode: continuous space vector modulation
  config.set(PWM_FREQ_OUT, PWMOutputFrequency::FREQ_25_KHZ);      // PWM frequency: 25 kHz
  config.set(PWM_DITHER_MODE, PWMDitherMode::RANDOM);             // PWM dither mode: random
  config.set(PWM_DITHER_DEPTH, PWMDitherDepth::DEPTH_5_PERCENT);  // PWM dither depth: 5%
  // N/A: PWM_DITHER_STEP: only for triangular PWM dither

  // Buck converter
  config.set(BUCK_DIS, true);     // Buck converter: disabled, FB_BK tied to +3.3 V rail
  config.set(BUCK_PS_DIS, true);  // Buck power sequencing: disabled, AVDD supplies from VM, DVDD supplied from FB_BK
  config.set(BUCK_SEL, BuckVoltage::OUTPUT_3_3_V);      // Buck voltage selection: 3.3 V
  config.set(BUCK_CL, BuckCurrentLimit::LIMIT_150_MA);  // Buck current limit: 150 mA, unclear if this setting is
                                                        // needed, suggested by SLLA673

  // Bus voltage and power limits
  config.set(BUS_VOLT, BusVoltage::BUS_30_V);                           // Bus voltage configuration: 30 V maximum
  config.set(VOLTAGE_HYSTERESIS, VoltageHysteresis::HYSTERESIS_1_0_V);  // Voltage limit hysteresis: 1 V
  config.set(MIN_VM_MOTOR, MinVMMotor::THR_10_V);                       // Minimum bus voltage to operate motor: 10 V
  config.set(MIN_VM_MODE, MinMaxVMMode::AUTO_CLEAR);  // Minimum bus voltage mode: clear fault when in bounds
  config.set(MAX_VM_MOTOR, MaxVMMotor::THR_18_V);     // Maximum bus voltage to operate motor: 18 V
  config.set(MAX_VM_MODE, MinMaxVMMode::AUTO_CLEAR);  // Maximum bus voltage mode: clear fault when in bounds
  config.set(VDC_FILTER, VDCFilter::DEFAULT);         // Vdc(VM) filter cut-off frequency: factory default
  config.set(MAX_POWER, max_power_from_watts(BOARD_DC_BUS_MAX_POWER_WATTS));  // Maximum bus power
  config.set(BUS_POWER_LIMIT_ENABLE, true);                                   // Limit DC bus to `MAX_POWER`: enabled

  // Protection circuits
  config.set(OVP_SEL, OvervoltageLevel::LIMIT_22_V);  // Bus overvoltage level: 22 V
  config.set(OVP_EN, true);                           // Bus overvoltage protection: enabled
  config.set(OTW_REP, true);  // Overtemperature warning report: enabled, fault at 145 C junction temperature
  config.set(OCP_DEG, OvercurrentDeglitch::TIME_0_6_US);  // Overcurrent deglitch time: 0.6 us
  config.set(OCP_LVL, OvercurrentLevel::LIMIT_16_A);      // Overcurrent level: 16 A
  config.set(OCP_MODE, OvercurrentMode::LATCHED_FAULT);   // Overcurrent fault mode: report a latched fault

  // Output pins
  config.set(ALARM_PIN_EN, false);  // Alarm pin: disabled
  config.set(
      FG_SEL, FGSelect::FG_IN_ISD_OPEN_AND_CLOSED_LOOP);  // FG output select: output in ISD, open loop, and closed loop
  config.set(
      FG_CONFIG, FGConfig::FG_WHILE_DRIVEN);  // FG output configuration: FG active as long as the motor is driven
  // N/A: FG_BEMF_THR: only for FG_CONFIG in BEMF threshold mode
  // N/A: FG_IDLE_CONFIG: nothing connected to FG pin
  // N/A: FG_FAULT_CONFIG: nothing connected to FG pin
  // N/A: DAC_SOx_SEL: nothing connected to DAC pins

  // EEPROM
  config.set(EEP_FAULT_MODE, EEPROMFaultMode::LATCHED_FAULT);              // EEPROM fault mode: latched fault
  config.set(EEPROM_LOCK_MODE, EEPROMLockMode::READ_OPEN_AND_WRITE_OPEN);  // EEPROM lock mode: disabled
  // N/A: EEPROM_LOCK_KEY: EEPROM is not locked

  // Miscellaneous
  config.set(SPEED_PIN_GLITCH_FILTER, SpeedPinGlitchFilter::TIME_0_2_US);  // Speed pin glitch filter: 0.2 us
  config.set(SLEW_RATE_I2C_PINS, SlewRateI2CPins::SLEW_4_8_MA);            // I2C slew rate: 4.8 mA, factory default
  config.set(PULLUP_ENABLE, false);                            // Internal pull-ups on FG and NFAULT: disabled
  config.set(CRC_ERR_MODE, CRCErrorMode::LATCHED_FAULT_HI_Z);  // CRC error mode: latched fault
  config.set(SATURATION_FLAGS_EN, false);  // Indicate current loop and speed loop saturation: disabled
  config.set(EXT_CLK_EN, false);           // External clock: disabled
  // N/A: EXT_CLK_CONFIG: no external clock to configure

  return config;
}

bool MotorController::set_inputs_(float speed_in_rotor_hz, bool exhaust, bool brake) {
  const bool have_speed = speed_in_rotor_hz > 0;
  this->inputs_active_ = have_speed || brake;

  bool error = false;
  error |= brake && driver()->write_brake_input_config(true);
  error |= !have_speed && driver()->write_speed_input(0);
  error |= driver()->write_direction_input_config(exhaust);
  error |= have_speed && driver()->write_speed_input(speed_in_rotor_hz);
  error |= !brake && driver()->write_brake_input_config(false);
  if (error) {
    this->inputs_active_ = true;  // just in case prior inputs remain active
  }
  return !error;
}

void MotorController::clear_inputs_and_sleep_() {
  if (this->inputs_active_) {
    driver()->write_speed_input(0);
    driver()->write_brake_input_config(false);
    this->inputs_active_ = false;
  }
  driver()->sleep();
}

void MotorController::init(const MotorDescriptor &descriptor) {
  this->shutdown();

  ESP_LOGI(minuet::TAG, "Initializing the fan motor driver with descriptor \"%s\"", descriptor.label);
  Config config = this->make_config_(descriptor.profile);
  log_config(config);

  ErrorCode error = driver()->write_config(config);
  if (!error) {
    error = driver()->save_config_to_eeprom();
  }
  if (error) {
    ESP_LOGE(minuet::TAG, "Failed to initialize the fan motor driver: %s", MCF8316Component::error_name(error));
    return;
  }

  this->profile_ = descriptor.profile;
  this->ready_ = true;

  // Put the driver to sleep to prevent a watchdog fault from being reported prematurely in case
  // the remaining firmware setup routine takes too long to complete.
  this->clear_inputs_and_sleep_();
}

void MotorController::shutdown() {
  if (!this->ready_) {
    return;
  }

  ESP_LOGI(minuet::TAG, "Shutting down the fan motor driver");
  if (driver()->is_awake()) {
    this->clear_inputs_and_sleep_();
  }
  this->ready_ = false;
}

bool MotorController::set_state(float speed_rpm, bool exhaust, bool brake, bool keep_awake) {
  ESP_LOGI(minuet::TAG, "Set fan motor inputs: speed_rpm=%.0f, exhaust=%d, brake=%d, keep_awake=%d", speed_rpm, exhaust,
      brake, keep_awake);
  const bool run = speed_rpm > 0 || brake;
  if (!this->ready_) {
    ESP_LOGW(minuet::TAG, "Fan motor driver not ready");
    return false;
  }

  if (driver()->config_shadow().needs_mpet_for_speed_loop()) {
    ESP_LOGW(minuet::TAG, "Must run MPET before starting the fan.");
    return false;  // don't poke the speed input
  }

  if (run || keep_awake) {
    driver()->wake();
  }

  if (driver()->is_awake() && !this->set_inputs_(rpm_to_hz(speed_rpm), exhaust, brake)) {
    ESP_LOGW(minuet::TAG, "Failed to set the fan driver inputs");
    if (!keep_awake) {
      this->clear_inputs_and_sleep_();
    }
    return false;
  }

  if (!run) {
    if (driver()->is_awake() && driver()->is_faulted()) {
      driver()->clear_fault();
    }
    if (!keep_awake) {
      this->clear_inputs_and_sleep_();
    }
  }
  return true;
}

void MotorController::start_mpet() {
  if (!this->ready_) {
    ESP_LOGW(minuet::TAG, "Fan motor not ready");
    return;
  }

  driver()->wake();
  driver()->clear_fault();
  driver()->start_mpet(true /*write_shadow*/);
}

float MotorController::get_tachometer_rpm() {
  if (this->ready_) {
    float speed_in_rotor_hz;
    ErrorCode error = driver()->read_speed_feedback(&speed_in_rotor_hz);
    if (!error) {
      return hz_to_rpm(speed_in_rotor_hz);
    }
  }
  return 0.f;
}

float MotorController::get_bus_current() {
  if (this->ready_) {
    float current_in_amps;
    ErrorCode error = driver()->read_bus_current(&current_in_amps);
    if (!error) {
      return current_in_amps;
    }
  }
  return 0.f;
}

float MotorController::get_motor_phase_peak_current() {
  if (this->ready_) {
    float current_in_amps;
    ErrorCode error = driver()->read_motor_phase_peak_current(&current_in_amps);
    if (!error) {
      return current_in_amps;
    }
  }
  return 0.f;
}

float MotorController::get_vm_voltage() {
  if (this->ready_) {
    float voltage_in_volts;
    ErrorCode error = driver()->read_vm_voltage(&voltage_in_volts);
    if (!error) {
      return voltage_in_volts;
    }
  }
  return 0.f;
}

float MotorController::get_fan_speed_rpm_for_index(int index) const {
  if (this->ready_ && index >= 1 && index <= 10) {
    return std::round(
               std::lerp(this->profile_.fan_speed_rpm_min, this->profile_.fan_speed_rpm_max, float(index - 1) / 9) *
               0.1f) *
        10;
  }
  return 0.f;
}

std::string MotorController::format_status() const {
  std::string result;
  auto fault_status = driver()->fault_status();
  if (fault_status.gate_driver) {
    result = esphome::mcf8316::format_gate_driver_fault_status(fault_status.gate_driver);
  }
  if (fault_status.controller) {
    if (!result.empty()) {
      result.append(" ");
    }
    result.append(esphome::mcf8316::format_controller_fault_status(fault_status.controller));
  }
  if (result.empty()) {
    result = this->ready_ ? "OK" : "NOT_READY";
  }
  return result;
}

MotorController motor_controller{};

bool was_algorithm_running_during_last_motor_update{};

}  // namespace fan
}  // namespace minuet
