#pragma once

#include <cstdint>
#include <string_view>

#include "cct_string.hpp"
#include "reader.hpp"
#include "writer.hpp"

namespace cct {

class File : public Reader, public Writer {
 public:
  enum class Type : int8_t { kCache, kSecret, kStatic, kLog };
  enum class IfError : int8_t { kThrow, kNoThrow };

  /// Creates a File directly from a file path.
  File(std::string_view filePath, IfError ifError);

  /// Creates a File from the coincenter data directory, with the type of the file and its name in the main data
  /// directory.
  File(std::string_view dataDir, Type type, std::string_view name, IfError ifError);

  [[nodiscard]] string readAll() const override;

  int write(std::string_view data, Writer::Mode mode = Writer::Mode::FromStart) const override;

  bool exists() const;

 private:
  string _filePath;
  IfError _ifError;
};

}  // namespace cct