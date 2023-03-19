#pragma once

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

class File {
 public:
  enum class Type : int8_t { kCache, kSecret, kStatic };
  enum class IfError : int8_t { kThrow, kNoThrow };

  File(std::string_view dataDir, Type type, std::string_view name, IfError ifError);

  string read() const;

  json readJson() const;

  void write(const json &data) const;

 private:
  string _filePath;
  IfError _ifError;
};

}  // namespace cct