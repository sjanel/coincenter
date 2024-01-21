#pragma once

#include <cstdint>
#include <string_view>

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "timedef.hpp"

namespace cct {

class PermanentCurlOptions {
 public:
  static constexpr auto kDefaultNbMaxRetries = 5;

  enum class TooManyErrorsPolicy : int8_t { kThrow, kReturnEmptyResponse };

  PermanentCurlOptions() noexcept = default;

  const auto &getUserAgent() const { return _userAgent; }

  const auto &getAcceptedEncoding() const { return _acceptedEncoding; }

  auto minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  auto followLocation() const { return _followLocation; }

  auto requestCallLogLevel() const { return _requestCallLogLevel; }
  auto requestAnswerLogLevel() const { return _requestAnswerLogLevel; }

  auto tooManyErrorsPolicy() const { return _tooManyErrorsPolicy; }

  auto nbMaxRetries() const { return _nbMaxRetries; }

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

    Builder &setNbMaxRetries(int nbMaxRetries) {
      _nbMaxRetries = nbMaxRetries;
      return *this;
    }

    Builder &setFollowLocation() {
      _followLocation = true;
      return *this;
    }

    Builder &setTooManyErrorsPolicy(TooManyErrorsPolicy tooManyErrorsPolicy) {
      _tooManyErrorsPolicy = tooManyErrorsPolicy;
      return *this;
    }

    PermanentCurlOptions build() {
      return {std::move(_userAgent), std::move(_acceptedEncoding), _minDurationBetweenQueries,
              _requestCallLogLevel,  _requestAnswerLogLevel,       _nbMaxRetries,
              _followLocation,       _tooManyErrorsPolicy};
    }

   private:
    string _userAgent;
    string _acceptedEncoding;
    Duration _minDurationBetweenQueries{};
    log::level::level_enum _requestCallLogLevel = log::level::level_enum::info;
    log::level::level_enum _requestAnswerLogLevel = log::level::level_enum::trace;
    int _nbMaxRetries = kDefaultNbMaxRetries;
    bool _followLocation = false;
    TooManyErrorsPolicy _tooManyErrorsPolicy = TooManyErrorsPolicy::kThrow;
  };

 private:
  PermanentCurlOptions(string userAgent, string acceptedEncoding, Duration minDurationBetweenQueries,
                       log::level::level_enum requestCallLogLevel, log::level::level_enum requestAnswerLogLevel,
                       int nbMaxRetries, bool followLocation, TooManyErrorsPolicy tooManyErrorsPolicy)
      : _userAgent(std::move(userAgent)),
        _acceptedEncoding(std::move(acceptedEncoding)),
        _minDurationBetweenQueries(minDurationBetweenQueries),
        _requestCallLogLevel(requestCallLogLevel),
        _requestAnswerLogLevel(requestAnswerLogLevel),
        _nbMaxRetries(nbMaxRetries),
        _followLocation(followLocation),
        _tooManyErrorsPolicy(tooManyErrorsPolicy) {}

  string _userAgent;
  string _acceptedEncoding;
  Duration _minDurationBetweenQueries;
  log::level::level_enum _requestCallLogLevel;
  log::level::level_enum _requestAnswerLogLevel;
  int _nbMaxRetries;
  bool _followLocation;
  TooManyErrorsPolicy _tooManyErrorsPolicy;
};

}  // namespace cct