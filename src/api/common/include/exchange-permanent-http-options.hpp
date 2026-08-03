#pragma once

#include <cstdint>

#include "exchange-query-config.hpp"
#include "permanentrequestoptions.hpp"

namespace cct::api {

class ExchangePermanentHttpOptions {
 public:
  explicit ExchangePermanentHttpOptions(const schema::ExchangeQueryConfig &queryConfig);

  enum class Api : int8_t { Public, Private };

  PermanentRequestOptions::Builder builderBase(Api api) const;

 private:
  const schema::ExchangeQueryConfig &_queryConfig;
};

}  // namespace cct::api