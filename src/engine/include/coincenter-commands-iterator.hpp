#pragma once

#include <span>

#include "coincentercommand.hpp"

namespace cct {

class CoincenterCommandsIterator {
 public:
  using CoincenterCommandSpan = std::span<const CoincenterCommand>;

  /// Initializes a new iterator on all coincenter commands.
  explicit CoincenterCommandsIterator(CoincenterCommandSpan commands = CoincenterCommandSpan()) noexcept;

  /// Returns 'true' if this iterator has still some command groups.
  bool hasNextCommandGroup() const;

  /// Get next grouped commands and advance the iterator.
  /// The grouped commands are guaranteed to have same type and make it possible to parallelize requests when possible.
  CoincenterCommandSpan nextCommandGroup();

 private:
  CoincenterCommandSpan _commands;
  CoincenterCommandSpan::size_type _pos;
};

}  // namespace cct