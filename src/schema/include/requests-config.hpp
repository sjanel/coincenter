#pragma once

namespace cct::schema {

struct ConcurrencyConfig {
  int nbMaxParallelRequests{1};
};

struct RequestsConfig {
  ConcurrencyConfig concurrency;
};

}  // namespace cct::schema