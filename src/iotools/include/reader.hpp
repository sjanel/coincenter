#pragma once

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct {

class Reader {
 public:
  // Read all content and return a string of it.
  virtual string readAll() const { return {}; }

  // Read all content, and constructs a json object from it.
  json readAllJson() const;

  virtual ~Reader() = default;
};

}  // namespace cct