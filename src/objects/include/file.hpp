#pragma once

#include "cct_json.hpp"
#include "cct_string.hpp"
#include "reader.hpp"
#include "writer.hpp"

namespace cct {

class File : public Reader, public Writer {
 public:
  enum class Type : int8_t { kCache, kSecret, kStatic, kLog };
  enum class IfError : int8_t { kThrow, kNoThrow };

  File(std::string_view dataDir, Type type, std::string_view name, IfError ifError);

  string readAll() const override;

  int write(const json &data, Writer::Mode mode = Writer::Mode::FromStart) const override;

 private:
  string _filePath;
  IfError _ifError;
};

}  // namespace cct