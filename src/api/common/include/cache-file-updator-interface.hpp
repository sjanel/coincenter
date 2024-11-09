#pragma once

namespace cct {

class CacheFileUpdatorInterface {
 public:
  virtual ~CacheFileUpdatorInterface() = default;

  virtual void updateCacheFile() const {}
};

}  // namespace cct