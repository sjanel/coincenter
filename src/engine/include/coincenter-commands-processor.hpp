#pragma once

#include <span>

#include "coincentercommand.hpp"
#include "queryresultprinter.hpp"
#include "transferablecommandresult.hpp"

namespace cct {

class Coincenter;
class CoincenterInfo;
class CoincenterCommands;

class CoincenterCommandsProcessor {
 public:
  explicit CoincenterCommandsProcessor(Coincenter &coincenter);

  /// Launch given commands and return the number of processed commands.
  int process(const CoincenterCommands &coincenterCommands);

 private:
  TransferableCommandResultVector processGroupedCommands(
      std::span<const CoincenterCommand> groupedCommands,
      std::span<const TransferableCommandResult> previousTransferableResults);

  Coincenter &_coincenter;
  QueryResultPrinter _queryResultPrinter;
};
}  // namespace cct
