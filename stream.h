#ifndef STREAM_H_
#define STREAM_H_

#include <iostream>

namespace util {

template <typename T>
class Stream {
 public:
  virtual bool Write(const T& token) = 0;
  int WriteBuffer(const T* buffer, int count) {
    int written = 0;
    for (; written < count; ++written) {
      if (!Write(*buffer++)) {
        return written;
      }
    }
    return count;
  }

  void RegisterDownstream(Stream<T>* stream) { stream_ = stream; }

 protected:
  Stream<T>* stream() { return stream_; }

 private:
  Stream<T>* stream_;
};

class OstreamAdapter : public Stream<uint8_t> {
 public:
  OstreamAdapter(std::ostream* ostream) : ostream_(ostream) {}

  bool Write(const uint8_t& token) final {
    ostream_->put(token);
    return true;
  }

 private:
  std::ostream* ostream_;
};

}  // namespace util

#endif  // STREAM_H_
