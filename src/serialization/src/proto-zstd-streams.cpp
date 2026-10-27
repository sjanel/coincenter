#include "proto-zstd-streams.hpp"

#include <google/protobuf/io/zero_copy_stream.h>
#include <zstd.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {

void ZstdOutputStream::CCtxDeleter::operator()(ZSTD_CCtx_s* pCtx) const noexcept { ZSTD_freeCCtx(pCtx); }

void ZstdInputStream::DCtxDeleter::operator()(ZSTD_DCtx_s* pCtx) const noexcept { ZSTD_freeDCtx(pCtx); }

ZstdOutputStream::ZstdOutputStream(::google::protobuf::io::ZeroCopyOutputStream* pSubStream, int compressionLevel)
    : _pSubStream(pSubStream), _cctx(ZSTD_createCCtx()), _inBufSize(ZSTD_CStreamInSize()) {
  if (_cctx == nullptr) {
    throw exception("Cannot create zstd compression context");
  }
  const std::size_t ret = ZSTD_CCtx_setParameter(_cctx.get(), ZSTD_c_compressionLevel, compressionLevel);
  if (ZSTD_isError(ret)) {
    throw exception("Cannot set zstd compression level {}: {}", compressionLevel, ZSTD_getErrorName(ret));
  }
  _inBuf = std::make_unique_for_overwrite<std::byte[]>(_inBufSize);
}

ZstdOutputStream::~ZstdOutputStream() {
  compressPending(Mode::kEnd);
  if (_outAvail != 0) {
    _pSubStream->BackUp(static_cast<int>(_outAvail));
  }
}

bool ZstdOutputStream::Next(void** data, int* size) {
  if (!compressPending(Mode::kContinue)) {
    return false;
  }
  // The whole input buffer is handed out. The part that is not backed up will be compressed at next call.
  _inPending = std::min(_inBufSize, static_cast<std::size_t>(std::numeric_limits<int>::max()));
  _byteCount += static_cast<int64_t>(_inPending);
  *data = _inBuf.get();
  *size = static_cast<int>(_inPending);
  return true;
}

void ZstdOutputStream::BackUp(int count) {
  assert(count >= 0 && static_cast<std::size_t>(count) <= _inPending);
  _inPending -= static_cast<std::size_t>(count);
  _byteCount -= count;
}

bool ZstdOutputStream::compressPending(Mode mode) {
  if (_failed) {
    return false;
  }
  if (mode == Mode::kContinue && _inPending == 0) {
    return true;
  }
  ZSTD_inBuffer input{_inBuf.get(), _inPending, 0};
  _inPending = 0;

  const ZSTD_EndDirective endDirective = mode == Mode::kEnd ? ZSTD_e_end : ZSTD_e_continue;

  std::size_t remaining;
  do {
    if (_outAvail == 0) {
      void* pOut;
      int outSize;
      if (!_pSubStream->Next(&pOut, &outSize)) {
        log::error("zstd output stream: cannot get a buffer from the underlying stream");
        _failed = true;
        return false;
      }
      _pOutBuf = static_cast<std::byte*>(pOut);
      _outAvail = static_cast<std::size_t>(outSize);
    }
    ZSTD_outBuffer output{_pOutBuf, _outAvail, 0};
    remaining = ZSTD_compressStream2(_cctx.get(), &output, &input, endDirective);
    if (ZSTD_isError(remaining)) {
      log::error("zstd compression error: {}", ZSTD_getErrorName(remaining));
      _failed = true;
      return false;
    }
    _pOutBuf += output.pos;
    _outAvail -= output.pos;
    // In end mode, 'remaining' is the number of bytes still to flush (0 when the frame is complete)
  } while (mode == Mode::kEnd ? remaining != 0 : input.pos != input.size);

  return true;
}

ZstdInputStream::ZstdInputStream(::google::protobuf::io::ZeroCopyInputStream* pSubStream)
    : _pSubStream(pSubStream), _dctx(ZSTD_createDCtx()), _outBufSize(ZSTD_DStreamOutSize()) {
  if (_dctx == nullptr) {
    throw exception("Cannot create zstd decompression context");
  }
  _outBuf = std::make_unique_for_overwrite<std::byte[]>(_outBufSize);
}

ZstdInputStream::~ZstdInputStream() {
  if (_inPos < _inSize) {
    _pSubStream->BackUp(static_cast<int>(_inSize - _inPos));
  }
}

bool ZstdInputStream::Next(const void** data, int* size) {
  if (_backedUp != 0) {
    *data = _outBuf.get() + (_outSize - _backedUp);
    *size = static_cast<int>(_backedUp);
    _byteCount += static_cast<int64_t>(_backedUp);
    _backedUp = 0;
    return true;
  }
  if (_failed) {
    return false;
  }
  while (true) {
    if (_inPos == _inSize) {
      const void* pIn;
      int inSize;
      if (!_pSubStream->Next(&pIn, &inSize)) {
        if (_frameInProgress) {
          log::error("zstd input stream: truncated data, the last frame is incomplete");
          _failed = true;
        }
        return false;
      }
      _pIn = static_cast<const std::byte*>(pIn);
      _inSize = static_cast<std::size_t>(inSize);
      _inPos = 0;
    }
    ZSTD_inBuffer input{_pIn, _inSize, _inPos};
    ZSTD_outBuffer output{_outBuf.get(), _outBufSize, 0};
    // Frames following each other in the stream are decompressed transparently
    const std::size_t ret = ZSTD_decompressStream(_dctx.get(), &output, &input);
    if (ZSTD_isError(ret)) {
      log::error("zstd decompression error: {}", ZSTD_getErrorName(ret));
      _failed = true;
      return false;
    }
    _inPos = input.pos;
    _frameInProgress = ret != 0;
    if (output.pos != 0) {
      _outSize = output.pos;
      _byteCount += static_cast<int64_t>(output.pos);
      *data = _outBuf.get();
      *size = static_cast<int>(output.pos);
      return true;
    }
  }
}

void ZstdInputStream::BackUp(int count) {
  assert(count >= 0 && static_cast<std::size_t>(count) <= _outSize);
  _backedUp = static_cast<std::size_t>(count);
  _byteCount -= count;
}

bool ZstdInputStream::Skip(int count) {
  while (count > 0) {
    const void* pData;
    int size;
    if (!Next(&pData, &size)) {
      return false;
    }
    if (size > count) {
      BackUp(size - count);
      return true;
    }
    count -= size;
  }
  return true;
}

}  // namespace cct
