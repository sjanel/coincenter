#pragma once

#include <google/protobuf/util/delimited_message_util.h>

#include <cstdint>
#include <istream>

#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
class ProtobufMessagesReader {
 public:
  explicit ProtobufMessagesReader(std::istream& is) : _is(is), _iis(&_is), _cis(&_iis) {}

  bool hasNext() { return _cis.ReadVarint64(&_nextSize); }

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
  std::istream& _is;
  ::google::protobuf::io::IstreamInputStream _iis;
  ::google::protobuf::io::CodedInputStream _cis;
  uint64_t _nextSize{};
};

template <class OStreamType>
class ProtobufMessagesWriter {
 public:
  void open(OStreamType&& newOs) {
    // reverse destroy streams to flush latest data. Recreate the streams after creation of new ofstream
    _cos.reset();
    _oos.reset();
    _os = std::move(newOs);
    _oos = std::make_unique<::google::protobuf::io::OstreamOutputStream>(&_os);
    _cos = std::make_unique<::google::protobuf::io::CodedOutputStream>(_oos.get());
  }

  template <class MsgT>
  void write(const MsgT& msg) {
    if (!_cos) {
      throw exception("ProtobufMessagesWriter::open should have been called first");
    }

    _cos->WriteVarint64(msg.ByteSizeLong());

    if (!msg.SerializeToCodedStream(_cos.get())) {
      log::error("Failed to serialize to coded stream");
    }
  }

  OStreamType flush() {
    _cos.reset();
    _oos.reset();

    OStreamType ret(std::move(_os));
    _os = OStreamType();
    return ret;
  }

 private:
  OStreamType _os;
  std::unique_ptr<::google::protobuf::io::OstreamOutputStream> _oos;
  std::unique_ptr<::google::protobuf::io::CodedOutputStream> _cos;
};
}  // namespace cct