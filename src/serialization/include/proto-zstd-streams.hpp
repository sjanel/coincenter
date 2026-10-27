#pragma once

#include <google/protobuf/io/zero_copy_stream.h>

#include <cstddef>
#include <cstdint>
#include <memory>

// Forward declarations of the zstd contexts to avoid exposing zstd.h in this header.
struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;

namespace cct {

/// ZeroCopyOutputStream compressing everything written to it with zstd into the given sub stream.
/// The zstd frame is terminated at destruction (the sub stream should outlive this object).
/// Several frames can be appended one after the other in the same sub stream, and are read back as a single stream by
/// ZstdInputStream.
class ZstdOutputStream final : public ::google::protobuf::io::ZeroCopyOutputStream {
 public:
  static constexpr int kDefaultCompressionLevel = 3;

  explicit ZstdOutputStream(::google::protobuf::io::ZeroCopyOutputStream* pSubStream,
                            int compressionLevel = kDefaultCompressionLevel);

  ZstdOutputStream(const ZstdOutputStream&) = delete;
  ZstdOutputStream(ZstdOutputStream&&) = delete;

  ZstdOutputStream& operator=(const ZstdOutputStream&) = delete;
  ZstdOutputStream& operator=(ZstdOutputStream&&) = delete;

  /// Ends the zstd frame, flushes it to the sub stream and releases the unused part of its last buffer.
  ~ZstdOutputStream() override;

  bool Next(void** data, int* size) override;

  void BackUp(int count) override;

  int64_t ByteCount() const override { return _byteCount; }

 private:
  struct CCtxDeleter {
    void operator()(ZSTD_CCtx_s* pCtx) const noexcept;
  };

  enum class Mode : int8_t { kContinue, kEnd };

  // Compresses the pending input (and ends the frame in kEnd mode) into the sub stream.
  bool compressPending(Mode mode);

  ::google::protobuf::io::ZeroCopyOutputStream* _pSubStream;
  std::unique_ptr<ZSTD_CCtx_s, CCtxDeleter> _cctx;
  std::unique_ptr<std::byte[]> _inBuf;
  std::byte* _pOutBuf{};
  int64_t _byteCount{};
  std::size_t _inBufSize;
  std::size_t _inPending{};
  std::size_t _outAvail{};
  bool _failed{};
};

/// ZeroCopyInputStream decompressing the zstd frame(s) read from the given sub stream.
class ZstdInputStream final : public ::google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit ZstdInputStream(::google::protobuf::io::ZeroCopyInputStream* pSubStream);

  ZstdInputStream(const ZstdInputStream&) = delete;
  ZstdInputStream(ZstdInputStream&&) = delete;

  ZstdInputStream& operator=(const ZstdInputStream&) = delete;
  ZstdInputStream& operator=(ZstdInputStream&&) = delete;

  /// Gives back the compressed bytes read but not consumed to the sub stream.
  ~ZstdInputStream() override;

  bool Next(const void** data, int* size) override;

  void BackUp(int count) override;

  bool Skip(int count) override;

  int64_t ByteCount() const override { return _byteCount; }

 private:
  struct DCtxDeleter {
    void operator()(ZSTD_DCtx_s* pCtx) const noexcept;
  };

  ::google::protobuf::io::ZeroCopyInputStream* _pSubStream;
  std::unique_ptr<ZSTD_DCtx_s, DCtxDeleter> _dctx;
  std::unique_ptr<std::byte[]> _outBuf;
  const std::byte* _pIn{};
  int64_t _byteCount{};
  std::size_t _outBufSize;
  std::size_t _outSize{};
  std::size_t _inSize{};
  std::size_t _inPos{};
  std::size_t _backedUp{};
  bool _frameInProgress{};
  bool _failed{};
};

}  // namespace cct
