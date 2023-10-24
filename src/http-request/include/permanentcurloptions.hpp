#pragma once

#include <string_view>

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

class PermanentCurlOptions {
 public:
  PermanentCurlOptions() noexcept = default;

  const string &getUserAgent() const { return _userAgent; }

  const string &getAcceptedEncoding() const { return _acceptedEncoding; }

  Duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  bool followLocation() const { return _followLocation; }

  log::level::level_enum requestCallLogLevel() const { return _requestCallLogLevel; }
  log::level::level_enum requestAnswerLogLevel() const { return _requestAnswerLogLevel; }

  class Builder {
   public:
    Builder() noexcept = default;

    Builder &setUserAgent(string userAgent) {
      _userAgent = std::move(userAgent);
      return *this;
    }

    Builder &setUserAgent(std::string_view userAgent) {
      _userAgent = string(userAgent);
      return *this;
    }

    Builder &setUserAgent(const char *userAgent) {
      _userAgent = string(userAgent);
      return *this;
    }

    Builder &setAcceptedEncoding(string acceptedEncoding) {
      _acceptedEncoding = std::move(acceptedEncoding);
      return *this;
    }

    Builder &setAcceptedEncoding(std::string_view acceptedEncoding) {
      _acceptedEncoding = string(acceptedEncoding);
      return *this;
    }

    Builder &setAcceptedEncoding(const char *acceptedEncoding) {
      _acceptedEncoding = string(acceptedEncoding);
      return *this;
    }

    Builder &setMinDurationBetweenQueries(Duration minDurationBetweenQueries) {
      _minDurationBetweenQueries = minDurationBetweenQueries;
      return *this;
    }

    Builder &setRequestCallLogLevel(log::level::level_enum requestCallLogLevel) {
      _requestCallLogLevel = requestCallLogLevel;
      return *this;
    }

    Builder &setRequestAnswerLogLevel(log::level::level_enum requestAnswerLogLevel) {
      _requestAnswerLogLevel = requestAnswerLogLevel;
      return *this;
    }

    Builder &setFollowLocation() {
      _followLocation = true;
      return *this;
    }

    PermanentCurlOptions build() {
      return {std::move(_userAgent), std::move(_acceptedEncoding), _minDurationBetweenQueries,
              _requestCallLogLevel,  _requestAnswerLogLevel,       _followLocation};
    }

   private:
    string _userAgent;
    string _acceptedEncoding;
    Duration _minDurationBetweenQueries{};
    log::level::level_enum _requestCallLogLevel = log::level::level_enum::info;
    log::level::level_enum _requestAnswerLogLevel = log::level::level_enum::trace;
    bool _followLocation = false;
  };

 private:
  PermanentCurlOptions(string userAgent, string acceptedEncoding, Duration minDurationBetweenQueries,
                       log::level::level_enum requestCallLogLevel, log::level::level_enum requestAnswerLogLevel,
                       bool followLocation)
      : _userAgent(std::move(userAgent)),
        _acceptedEncoding(std::move(acceptedEncoding)),
        _minDurationBetweenQueries(minDurationBetweenQueries),
        _requestCallLogLevel(requestCallLogLevel),
        _requestAnswerLogLevel(requestAnswerLogLevel),
        _followLocation(followLocation) {}

  string _userAgent;
  string _acceptedEncoding;
  Duration _minDurationBetweenQueries;
  log::level::level_enum _requestCallLogLevel;
  log::level::level_enum _requestAnswerLogLevel;
  bool _followLocation;
};

}  // namespace cct