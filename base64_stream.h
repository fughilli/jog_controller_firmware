#ifndef BASE64_STREAM_H_
#define BASE64_STREAM_H_

#include <stdint.h>

#include "bit_pipe.h"
#include "stream.h"

namespace util {

class Base64EncodeStream : public Stream<uint8_t> {
 public:
  bool Write(const uint8_t& token) final;
  bool Flush();

 private:
  BitPipe<uint16_t> pipe_{ShiftDirection::kLeft};
};

}  // namespace util

#endif  // BASE64_STREAM_H_
