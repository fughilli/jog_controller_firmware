#ifndef SWITCHES_H_
#define SWITCHES_H_

#include <Adafruit_MCP23017.h>

#include "keypad.h"

namespace jog_controller {

enum class RotarySwitch { kAxis = 0, kMultiplier };

class Switches {
 public:
  static constexpr int kEstopIndex = 0;
  static constexpr int kFeedholdIndex = 1;

  using RotarySwitchHandler = void (*)(RotarySwitch, int);

  Switches(TwoWire* bus, uint8_t address, int interrupt_a_pin,
           int interrupt_b_pin)
      : bus_(bus),
        address_(address),
        interrupt_a_pin_(interrupt_a_pin),
        interrupt_b_pin_(interrupt_b_pin),
        interrupt_triggered_(0), interrupt_triggered_follow_(0) {}

  void Begin();

  void RegisterRotarySwitchHandler(RotarySwitchHandler handler) {
    rotary_switch_handler_ = handler;
  }
  void RegisterKeyHandler(Keypad::KeyHandler handler) {
    key_handler_ = handler;
  }

  void Poll();

  void SetLedState(bool state);

 private:
  Adafruit_MCP23017 io_expander_;

  RotarySwitchHandler rotary_switch_handler_;
  Keypad::KeyHandler key_handler_;

  int interrupt_a_pin_;
  int interrupt_b_pin_;
  uint8_t interrupt_triggered_;
  uint8_t interrupt_triggered_follow_;
  TwoWire* bus_;
  uint8_t address_;
  int current_axis_index_ = 0;
  int current_multiplier_index_ = 0;
  bool current_feedhold_ = false;
  bool current_estop_ = false;

  static void StaticIsr();
  void Isr();
};

}  // namespace jog_controller

#endif  // SWITCHES_H_
