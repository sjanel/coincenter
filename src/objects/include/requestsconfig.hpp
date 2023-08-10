#pragma once

namespace cct {

class RequestsConfig {
 public:
  explicit RequestsConfig(int nbMaxParallelRequests = 1);

  int nbMaxParallelRequests(int nbMaxAccounts) const;

 private:
  int _nbMaxParallelRequests;
};

}  // namespace cct