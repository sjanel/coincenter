#pragma once

#include <string_view>

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

class File {
 public:
  enum class Type { kCache, kSecret, kStatic };
  enum class IfNotFound { kThrow, kNoThrow };

  constexpr File(Type type, std::string_view name, IfNotFound ifNotFound)
      : _name(name), _type(type), _ifNotFound(ifNotFound) {}

  string read() const;

  json readJson() const;

  void write(const json &data) const;

  constexpr std::string_view name() const { return _name; }

 private:
  std::string_view _name;
  Type _type;
  IfNotFound _ifNotFound;
};

}  // namespace cct