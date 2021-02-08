#ifndef BITS_H_
#define BITS_H_

#include <limits>
#include <type_traits>

namespace util {

template <typename T>
T BitRun(int bits) {
  static_assert(std::is_unsigned<T>::value, "");
  return (bits)
             ? (static_cast<T>(-1) >> (std::numeric_limits<T>::digits - bits))
             : 0;
}

template <typename T>
T MakeFieldMask(int bits, int offset) {
  return BitRun<T>(bits) << offset;
}

template <typename T>
T MakeField(T value, int bits, int offset) {
  return (BitRun<T>(bits) & value) << offset;
}

template <typename T>
T OverwriteField(T reg, T value, int bits, int offset) {
  return (reg & ~MakeFieldMask<T>(bits, offset)) |
         MakeField<T>(value, bits, offset);
}

template <typename T>
T GetField(T reg, int bits, int offset) {
  return (reg >> offset) & BitRun<T>(bits);
}

}  // namespace util

#endif  // BITS_H_
