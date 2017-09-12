#ifndef ISTREAMMOCK_H
#define ISTREAMMOCK_H

#include "IStream.h"

using mip::file::IStream;

class IStreamMock
    : public IStream,
      public std::enable_shared_from_this<IStreamMock> {
public:

  virtual int64_t Read(uint8_t *buffer, int64_t bufferLength) = 0;
  virtual int64_t Write(const uint8_t *buffer, int64_t bufferLength) = 0;
  virtual bool Flush() = 0;
  virtual void Seek(uint64_t position) = 0;
  virtual bool CanRead() const = 0;
  virtual bool CanWrite() const = 0;
  virtual uint64_t Position() = 0;
  virtual uint64_t Size() = 0;

  virtual ~IStreamMock() override;
};

#endif // ISTREAMMOCK_H
