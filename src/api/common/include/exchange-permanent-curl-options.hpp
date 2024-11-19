#pragma once

#include <cstdint>

#include "exchange-query-config.hpp"
#include "permanentcurloptions.hpp"

namespace cct::api {

class ExchangePermanentCurlOptions {
 public:
  explicit ExchangePermanentCurlOptions(const schema::ExchangeQueryConfig &queryConfig);

  enum class Api : int8_t { Public, Private };

  PermanentCurlOptions::Builder builderBase(Api api) const;

 private:
  const schema::ExchangeQueryConfig &_queryConfig;
};

}  // namespace cct::api