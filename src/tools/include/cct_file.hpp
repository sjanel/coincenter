#pragma once

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

class File {
 public:
  enum class Type { kCache, kSecret, kStatic };
  enum class IfNotFound { kThrow, kNoThrow };

  File(std::string_view dataDir, Type type, std::string_view name, IfNotFound ifNotFound);

  string read() const;

  json readJson() const;

  void write(const json &data) const;

 private:
  string _filePath;
  IfNotFound _ifNotFound;
};

}  // namespace cct