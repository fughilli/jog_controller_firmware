#ifndef PB_STREAM_H_
#define PB_STREAM_H_

#include "pb_encode.h"
#include "stream.h"

namespace util {

bool PbStreamCallback(pb_ostream_t* stream, const pb_byte_t* buf,
                      size_t count) {
  return reinterpret_cast<util::Stream<uint8_t>*>(stream->state)
      ->WriteBuffer(buf, count);
}

pb_ostream_t WrapStream(util::Stream<uint8_t>* stream) {
  return {
      .callback = &PbStreamCallback,
      .state = stream,
      .max_size = PB_SIZE_MAX,
  };
}

}  // namespace util

#endif  // PB_STREAM_H_
