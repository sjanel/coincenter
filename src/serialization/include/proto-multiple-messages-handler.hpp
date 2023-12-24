#pragma once

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <istream>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {

/// Utility class that allows compressed data to be written to rolling files.
/// At default constructor, no file is opened. Client is expected to call 'open' with a new std::ostream prior to any
/// write. Data is written with the following scheme:
///  - First a VarInt64 is written with the size (in bytes) of the object to be serialized.
///  - Then the object itself is serialized.
template <class OStreamType>
class ProtobufMessagesCompressedWriter {
 private:
  static_assert(std::derived_from<OStreamType, std::ostream>,
                "OStreamType for ProtobufMessagesCompressedWriter should derive from std::ostream");

  using OstreamOutputStream = ::google::protobuf::io::OstreamOutputStream;
  using GzipOutputStream = ::google::protobuf::io::GzipOutputStream;
  using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;

 public:
  /// Initializes a new ProtobufMessagesCompressedWriter without any opened stream.
  /// 'open' method should be called before any write.
  ProtobufMessagesCompressedWriter() noexcept(std::is_nothrow_default_constructible_v<OStreamType>) = default;

  /// No copy / move operations allowed as already deleted by underlying OstreamOutputStream & CodedOutputStream
  ProtobufMessagesCompressedWriter(const ProtobufMessagesCompressedWriter&) = delete;
  ProtobufMessagesCompressedWriter(ProtobufMessagesCompressedWriter&&) = delete;

  ProtobufMessagesCompressedWriter& operator=(const ProtobufMessagesCompressedWriter&) = delete;
  ProtobufMessagesCompressedWriter& operator=(ProtobufMessagesCompressedWriter&&) = delete;

  ~ProtobufMessagesCompressedWriter() { close(); }

  void open(OStreamType&& newOs) {
    close();

    _os = std::move(newOs);

    OstreamOutputStream* pOos;
    try {
      pOos = std::construct_at(getOstreamOutputStreamPtr(), std::addressof(_os));
    } catch (const std::exception& ex) {
      _os = OStreamType();
      throw ex;
    }

    GzipOutputStream* pGzos;
    try {
      pGzos = std::construct_at(getGzipOutputStreamPtr(), pOos);
    } catch (const std::exception& ex) {
      std::destroy_at(pOos);
      _os = OStreamType();
      throw ex;
    }

    try {
      std::construct_at(getCodedOutputStreamPtr(), pGzos);
    } catch (const std::exception& ex) {
      std::destroy_at(pGzos);
      std::destroy_at(pOos);
      _os = OStreamType();
      throw ex;
    }

    _isFileOpened = true;
  }

  template <class MsgT>
  void write(const MsgT& msg) {
    if (!_isFileOpened) {
      throw exception("ProtobufMessagesWriter::open should have been called first");
    }

    auto* cos = getCodedOutputStreamPtr();

    cos->WriteVarint32(static_cast<uint32_t>(msg.ByteSizeLong()));

    if (!msg.SerializeToCodedStream(cos)) {
      log::error("Failed to serialize to coded stream");
    }
  }

  OStreamType flush() noexcept(std::is_nothrow_swappable_v<OStreamType>) {
    close();

    OStreamType ret;
    ret.swap(_os);
    return ret;
  }

 private:
  void close() noexcept {
    if (_isFileOpened) {
      // reverse destroy streams to flush latest data. Recreate the streams after creation of new ofstream
      std::destroy_at(getCodedOutputStreamPtr());
      std::destroy_at(getGzipOutputStreamPtr());
      std::destroy_at(getOstreamOutputStreamPtr());
      _isFileOpened = false;
    }
  }

  OstreamOutputStream* getOstreamOutputStreamPtr() { return reinterpret_cast<OstreamOutputStream*>(_oos); }
  GzipOutputStream* getGzipOutputStreamPtr() { return reinterpret_cast<GzipOutputStream*>(_gzos); }
  CodedOutputStream* getCodedOutputStreamPtr() { return reinterpret_cast<CodedOutputStream*>(_cos); }

  OStreamType _os;
  alignas(OstreamOutputStream) std::byte _oos[sizeof(OstreamOutputStream)];
  alignas(GzipOutputStream) std::byte _gzos[sizeof(GzipOutputStream)];
  alignas(CodedOutputStream) std::byte _cos[sizeof(CodedOutputStream)];
  bool _isFileOpened = false;
};

class ProtobufMessageReaderBase {
 public:
  explicit ProtobufMessageReaderBase(google::protobuf::io::ZeroCopyInputStream* is) : _cis(is) {}

  /// Tells whether this reader has at least one more message to be read.
  bool hasNext() { return _cis.ReadVarintSizeAsInt(&_nextSize); }

  /// Read next message and returns it.
  /// 'hasNext' should have been called before, and returned true.
  template <class MsgT>
  MsgT next() {
    MsgT msg;
    auto msgLimit = _cis.PushLimit(_nextSize);
    if (!msg.ParseFromCodedStream(&_cis)) {
      log::error("Error reading single protobuf message of size {}", _nextSize);
    }
    _cis.PopLimit(msgLimit);
    return msg;
  }

 private:
  ::google::protobuf::io::CodedInputStream _cis;
  int _nextSize{};
};

/// Uncompressed messages reader iterator, provided as an example. Unused in production code, but can be useful if some
/// data has been written uncompressed.
class ProtobufMessageReaderIterator {
 public:
  explicit ProtobufMessageReaderIterator(std::istream& is) : _iis(&is), _protobufMessageReaderBase(&_iis) {}

  /// Tells whether this reader has at least one more message to be read.
  bool hasNext() { return _protobufMessageReaderBase.hasNext(); }

  /// Read next message and returns it.
  /// 'hasNext' should have been called before, and returned true.
  template <class MsgT>
  MsgT next() {
    return _protobufMessageReaderBase.next<MsgT>();
  }

 private:
  ::google::protobuf::io::IstreamInputStream _iis;
  ProtobufMessageReaderBase _protobufMessageReaderBase;
};

/// The compressed reader iterator that should be used in case files have been written with a
/// ProtobufMessagesCompressedWriter.
class ProtobufMessageCompressedReaderIterator {
 public:
  explicit ProtobufMessageCompressedReaderIterator(std::istream& is)
      : _iis(&is), _gzipIs(&_iis), _protobufMessageReaderBase(&_gzipIs) {}

  /// Tells whether this reader has at least one more message to be read.
  bool hasNext() { return _protobufMessageReaderBase.hasNext(); }

  /// Read next message and returns it.
  /// 'hasNext' should have been called before, and returned true.
  template <class MsgT>
  MsgT next() {
    return _protobufMessageReaderBase.next<MsgT>();
  }

 private:
  ::google::protobuf::io::IstreamInputStream _iis;
  ::google::protobuf::io::GzipInputStream _gzipIs;
  ProtobufMessageReaderBase _protobufMessageReaderBase;
};

}  // namespace cct