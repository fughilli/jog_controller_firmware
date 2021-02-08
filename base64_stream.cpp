#include "base64_stream.h"

namespace util {
namespace {
const uint8_t kForwardLookup[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const uint8_t kReverseLookup[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  62, 0,  0,  0,  63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,
    0,  0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 0,  0,  0,  0,  0};
const uint8_t kTerminator[] = "==";

}  // namespace

bool Base64EncodeStream::Write(const uint8_t& token) {
  pipe_.Push(token, 8);
  while (pipe_.size() >= 6) {
    uint16_t index = 0;
    if (!pipe_.Pop(6, &index)) {
      return false;
    }
    if (!stream()->Write(kForwardLookup[index])) {
      return false;
    }
  }
  return true;
}

bool Base64EncodeStream::Flush() {
  uint16_t index = 0;
  switch (pipe_.size()) {
    case 0:
      return true;

    case 2: {
      if (pipe_.Push(0, 4) && pipe_.Pop(6, &index) &&
          stream()->Write(kForwardLookup[index]) &&
          stream()->WriteBuffer(kTerminator, 2)) {
        return true;
      }
    } break;

    case 4: {
      if (pipe_.Push(0, 2) && pipe_.Pop(6, &index) &&
          stream()->Write(kForwardLookup[index]) &&
          stream()->WriteBuffer(kTerminator, 1)) {
        return true;
      }
    } break;

    default:
      return false;
  }
  return false;
}

}  // namespace util
