#include "switches.h"

#include <Arduino.h>

#include "bits.h"

namespace jog_controller {
namespace {
Switches* switches_instance_ = nullptr;

constexpr int kLedPin = 0;
constexpr int kFeedholdPin = 6;
constexpr int kEstopPin = 7;

constexpr int kAxisFieldSize = 6;
constexpr int kAxisFieldOffset = 8;

constexpr int kMultiplierFieldSize = 2;
constexpr int kMultiplierFieldOffset = 14;

int ExtractAxisIndex(uint16_t mask) {
  uint8_t axis_mask =
      util::GetField<uint16_t>(mask, kAxisFieldSize, kAxisFieldOffset);
  int count =
      util::CountTrailingOnesField<uint8_t>(axis_mask, kAxisFieldSize, 0);
  return (count + 1) % (kAxisFieldSize + 1);
}

int ExtractMultiplierIndex(uint16_t mask) {
  uint8_t multiplier_mask = util::GetField<uint16_t>(mask, kMultiplierFieldSize,
                                                     kMultiplierFieldOffset);
  int count = util::CountTrailingOnesField<uint8_t>(multiplier_mask,
                                                    kMultiplierFieldSize, 0);
  return (count + 1) % (kMultiplierFieldSize + 1);
}

}  // namespace

void Switches::Isr() {
  if (key_handler_ == nullptr || rotary_switch_handler_ == nullptr) {
    return;
  }

  ++interrupt_triggered_;
}

void Switches::StaticIsr() { switches_instance_->Isr(); }

void Switches::SetLedState(bool state) {
  io_expander_.digitalWrite(kLedPin, state ? LOW : HIGH);
}

void Switches::Begin() {
  switches_instance_ = this;
  io_expander_.begin(0, bus_);

  pinMode(interrupt_a_pin_, INPUT_PULLUP);
  pinMode(interrupt_b_pin_, INPUT_PULLUP);
  attachInterrupt(interrupt_a_pin_, &Switches::StaticIsr, FALLING);
  attachInterrupt(interrupt_b_pin_, &Switches::StaticIsr, FALLING);

  io_expander_.pinMode(kLedPin, OUTPUT);
  SetLedState(true);

  // All of the switches are active-low.
  for (int i = 6; i <= 15; ++i) {
    io_expander_.pullUp(i, 1);
    io_expander_.setupInterruptPin(i, CHANGE);
  }

  // Don't mirror, open drain, low active state for IOA, IOB
  io_expander_.setupInterrupts(false, true, LOW);
}

void Switches::Poll() {
  if (interrupt_triggered_ == interrupt_triggered_follow_) {
    return;
  }

  uint16_t mask = io_expander_.readGPIOAB();

  int axis_index = ExtractAxisIndex(mask);
  int multiplier_index = ExtractMultiplierIndex(mask);

  if (axis_index != current_axis_index_) {
    rotary_switch_handler_(RotarySwitch::kAxis, axis_index);
    current_axis_index_ = axis_index;
  }

  if (multiplier_index != current_multiplier_index_) {
    rotary_switch_handler_(RotarySwitch::kMultiplier, multiplier_index);
    current_multiplier_index_ = multiplier_index;
  }

  bool estop = util::GetField<uint16_t>(mask, 1, kEstopPin);
  bool feedhold = util::GetField<uint16_t>(mask, 1, kFeedholdPin);

  if (estop != current_estop_) {
    key_handler_(kEstopIndex,
                 current_estop_ ? KeyState::kReleased : KeyState::kPressed);
    current_estop_ = estop;
  }

  if (feedhold != current_feedhold_) {
    key_handler_(kFeedholdIndex,
                 current_feedhold_ ? KeyState::kReleased : KeyState::kPressed);
    current_feedhold_ = feedhold;
  }

  ++interrupt_triggered_follow_;
}

}  // namespace jog_controller
