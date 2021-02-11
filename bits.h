#ifndef BITS_H_
#define BITS_H_

#include <limits>
#include <type_traits>

namespace util {

template <typename T>
T AllOnes() {
  static_assert(std::is_unsigned<T>::value, "");
  return static_cast<T>(-1);
}

template <typename T>
T BitRun(int bits) {
  static_assert(std::is_unsigned<T>::value, "");
  return (bits) ? (AllOnes<T>() >> (std::numeric_limits<T>::digits - bits)) : 0;
}

template <typename T>
T LargestSingleBit() {
  return ~(AllOnes<T>() >> 1);
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

template <typename T>
int CountLeadingZeros(T value) {
  T compare_mask = LargestSingleBit<T>();
  int count = 0;
  while (compare_mask) {
    if (compare_mask & value) {
      return count;
    }

    ++count;
    compare_mask >>= 1;
  }

  return count;
}

}  // namespace util

#endif  // BITS_H_
