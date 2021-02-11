#ifndef KEYPAD_H_
#define KEYPAD_H_

#include <Wire.h>

namespace jog_controller {

enum class KeyState { kReleased = 0, kPressed };

class Keypad {
 public:
  // Function signature for a key handler.
  using KeyHandler = void (*)(int, KeyState);

  Keypad(TwoWire* bus, uint8_t address, int interrupt_pin)
      : bus_(bus),
        address_(address),
        interrupt_pin_(interrupt_pin),
        interrupt_triggered_(0) {}

  // Initializes the keypad driver and sets up interrupts.
  void Begin();

  // Registers a key handler for this keypad.
  void RegisterKeyHandler(KeyHandler handler) { handler_ = handler; }

  // If an interrupt was triggered prior to calling Poll, polls the PCF8574 for
  // four cycles to get the indices of pressed/released key(s). Invokes the
  // registered key handler for each pressed/released key.
  void Poll();

 private:
  static constexpr int kNumRows = 4;
  static constexpr int kNumCols = 4;

  // Configures the interrupt pin to respond to falling edges from the PCF8574
  // INT line.
  void ArmInterrupt();

  // Reset the state of the pins on the I/O expander.
  void ResetPins() { WriteRowPins(0x0f); }

  // Writes the row pins of the I/O expander to the given mask value. The upper
  // 4 bits of `row_mask` are ignored.
  void WriteRowPins(uint8_t row_mask);

  // Reads the column pins of the I/O expander, returning the result in
  // col_mask. Returns false on error.
  bool ReadColPins(uint8_t* col_mask);

  static void StaticIsr();
  void Isr();

  TwoWire* bus_;
  uint8_t address_;
  int interrupt_pin_;
  int interrupt_triggered_;
  KeyState key_states_[kNumRows * kNumCols] = {};
  KeyHandler handler_ = nullptr;
};

}  // namespace jog_controller

#endif  // KEYPAD_H_
