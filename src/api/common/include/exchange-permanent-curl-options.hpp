#pragma once

#include <cstdint>

#include "permanentcurloptions.hpp"

namespace cct {

class ExchangeConfig;

namespace api {
class ExchangePermanentCurlOptions {
 public:
  explicit ExchangePermanentCurlOptions(const ExchangeConfig &exchangeConfig);

  enum class Api : int8_t { Public, Private };

  PermanentCurlOptions::Builder builderBase(Api api) const;

 private:
  const ExchangeConfig &_exchangeConfig;
};
}  // namespace api

}  // namespace cct