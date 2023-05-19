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

  class Builder {
   public:
    Builder() noexcept = default;

    Builder &setUserAgent(std::string_view userAgent) {
      _userAgent = string(userAgent);
      return *this;
    }

    Builder &setAcceptedEncoding(std::string_view acceptedEncoding) {
      _acceptedEncoding = string(acceptedEncoding);
      return *this;
    }

    Builder &setMinDurationBetweenQueries(Duration minDurationBetweenQueries) {
      _minDurationBetweenQueries = minDurationBetweenQueries;
      return *this;
    }

    PermanentCurlOptions build() const {
      return PermanentCurlOptions(_userAgent, _acceptedEncoding, _minDurationBetweenQueries);
    }

   private:
    string _userAgent;
    string _acceptedEncoding;
    Duration _minDurationBetweenQueries{};
  };

 private:
  PermanentCurlOptions(std::string_view userAgent, std::string_view acceptedEncoding,
                       Duration minDurationBetweenQueries)
      : _userAgent(userAgent),
        _acceptedEncoding(acceptedEncoding),
        _minDurationBetweenQueries(minDurationBetweenQueries) {}

  string _userAgent;
  string _acceptedEncoding;
  Duration _minDurationBetweenQueries{};
};

}  // namespace cct