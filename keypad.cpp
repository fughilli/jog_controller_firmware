#include "keypad.h"

#include <Arduino.h>

#include "bits.h"

namespace jog_controller {

namespace {
Keypad* keypad_instance_ = nullptr;

constexpr int kRowFieldOffset = 0;
constexpr int kColFieldOffset = 4;
}  // namespace

void Keypad::StaticIsr() { keypad_instance_->Isr(); }

void Keypad::ArmInterrupt() {
  keypad_instance_ = this;
  pinMode(interrupt_pin_, INPUT_PULLUP);
  attachInterrupt(interrupt_pin_, &Keypad::StaticIsr, FALLING);
}

void Keypad::Isr() {
  if (handler_ == nullptr) {
    return;
  }

  interrupt_triggered_ = 1;
}

void Keypad::Poll() {
  if (interrupt_triggered_ == 0) {
    return;
  }

  for (int i = 0; i < kNumRows; ++i) {
    WriteRowPins(1 << i);

    uint8_t col_mask = 0;
    if (!ReadColPins(&col_mask)) {
      break;
    }

    for (int j = 0; j < kNumCols; ++j) {
      int button_index = j + i * kNumCols;
      if (col_mask & (1 << j)) {
        if (key_states_[button_index] == KeyState::kReleased) {
          key_states_[button_index] = KeyState::kPressed;
          handler_(button_index, KeyState::kPressed);
        }
      } else {
        if (key_states_[button_index] == KeyState::kPressed) {
          key_states_[button_index] = KeyState::kReleased;
          handler_(button_index, KeyState::kReleased);
        }
      }
    }
  }

  ResetPins();
  interrupt_triggered_ = 0;
}

void Keypad::WriteRowPins(uint8_t row_mask) {
  bus_->beginTransmission(address_);
  bus_->write(~util::MakeField<uint8_t>(row_mask, kNumRows, kRowFieldOffset));
  bus_->endTransmission();
}

void Keypad::Begin() {
  ResetPins();
  ArmInterrupt();
}

bool Keypad::ReadColPins(uint8_t* col_mask) {
  if (col_mask == nullptr) {
    return false;
  }
  // Start read transmission.
  bus_->requestFrom(address_, 1);

  auto start = micros();
  while (bus_->available() != 1) {
    if ((micros() - start) > 1000) {
      return false;
    }
  }

  *col_mask = util::GetField<uint8_t>(~bus_->read(), kNumCols, kColFieldOffset);
  return true;
}

}  // namespace jog_controller
