#pragma once

#include <string_view>

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

    Builder &setFollowLocation() {
      _followLocation = true;
      return *this;
    }

    PermanentCurlOptions build() {
      return {std::move(_userAgent), std::move(_acceptedEncoding), _minDurationBetweenQueries, _followLocation};
    }

   private:
    string _userAgent;
    string _acceptedEncoding;
    Duration _minDurationBetweenQueries{};
    bool _followLocation = false;
  };

 private:
  PermanentCurlOptions(string userAgent, string acceptedEncoding, Duration minDurationBetweenQueries,
                       bool followLocation)
      : _userAgent(std::move(userAgent)),
        _acceptedEncoding(std::move(acceptedEncoding)),
        _minDurationBetweenQueries(minDurationBetweenQueries),
        _followLocation(followLocation) {}

  string _userAgent;
  string _acceptedEncoding;
  Duration _minDurationBetweenQueries;
  bool _followLocation;
};

}  // namespace cct