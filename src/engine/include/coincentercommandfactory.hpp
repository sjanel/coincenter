#pragma once

#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"

namespace cct {
class StringOptionParser;

class CoincenterCommandFactory {
 public:
  CoincenterCommandFactory(const CoincenterCmdLineOptions &cmdLineOptions, const CoincenterCommand *pPreviousCommand)
      : _cmdLineOptions(cmdLineOptions), _pPreviousCommand(pPreviousCommand) {}

  static CoincenterCommand CreateMarketCommand(StringOptionParser &optionParser);

  CoincenterCommand createOrderCommand(CoincenterCommandType type, StringOptionParser &optionParser);

  CoincenterCommand createTradeCommand(CoincenterCommandType type, StringOptionParser &optionParser);

  CoincenterCommand createWithdrawApplyCommand(StringOptionParser &optionParser);

  CoincenterCommand createWithdrawApplyAllCommand(StringOptionParser &optionParser);

 private:
  const CoincenterCmdLineOptions &_cmdLineOptions;
  const CoincenterCommand *_pPreviousCommand;
};
}  // namespace cct