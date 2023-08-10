#include "requestsconfig.hpp"

#include <algorithm>

#include "cct_invalid_argument_exception.hpp"

namespace cct {
RequestsConfig::RequestsConfig(int nbMaxParallelRequests) : _nbMaxParallelRequests(nbMaxParallelRequests) {
  if (nbMaxParallelRequests < 1) {
    throw invalid_argument("Maximum number of parallel requests should be at least 1");
  }
}

int RequestsConfig::nbMaxParallelRequests(int nbMaxAccounts) const {
  return std::min(_nbMaxParallelRequests, nbMaxAccounts);
}

}  // namespace cct