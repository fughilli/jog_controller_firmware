#ifndef BIT_PIPE_H_
#define BIT_PIPE_H_

#include <limits>

#include "bits.h"

namespace util {

enum class ShiftDirection {
  kLeft = 0,
  kRight,
};

template <typename T>
class BitPipe {
 public:
  BitPipe(ShiftDirection shift_direction)
      : shift_direction_(shift_direction), size_(0), buffer_(0) {}

  BitPipe() : BitPipe(ShiftDirection::kLeft) {}

  bool Push(T value, int bits) {
    if ((size_ + bits) > std::numeric_limits<T>::digits) {
      return false;
    }
    switch (shift_direction_) {
      case ShiftDirection::kLeft:
        buffer_ <<= bits;
        buffer_ |= (BitRun<T>(bits) & value);
        break;

      case ShiftDirection::kRight:
        buffer_ = OverwriteField(buffer_, value, bits, size_);
        break;
    }
    size_ += bits;
    return true;
  }

  bool Pop(int bits, T* value) {
    if (bits > size_) {
      return false;
    }

    switch (shift_direction_) {
      case ShiftDirection::kLeft:
        if (value) {
          *value = GetField<T>(buffer_, bits, size_ - bits);
        }
        break;

      case ShiftDirection::kRight:
        if (value) {
          *value = buffer_ & BitRun<T>(bits);
        }
        buffer_ >>= bits;
        break;
    }
    size_ -= bits;
    return true;
  }

  int size() const { return size_; }

 private:
  ShiftDirection shift_direction_;
  int size_;
  T buffer_;
};

}  // namespace util

#endif  // BIT_PIPE_H_
