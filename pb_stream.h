#ifndef PB_STREAM_H_
#define PB_STREAM_H_

#include "stream.h"
#include "pb_encode.h"

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

}

#endif  // PB_STREAM_H_
