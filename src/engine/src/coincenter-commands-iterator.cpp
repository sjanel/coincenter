#include "coincenter-commands-iterator.hpp"

#include <bitset>

#include "cct_const.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "exchangename.hpp"

namespace cct {

CoincenterCommandsIterator::CoincenterCommandsIterator(CoincenterCommandSpan commands) noexcept
    : _commands(commands), _pos() {}

namespace {
using PublicExchangePresenceBitset = std::bitset<kNbSupportedExchanges>;

bool UpdateBitsetAreNewExchanges(const CoincenterCommand &command,
                                 PublicExchangePresenceBitset &publicExchangePresence) {
  if (command.exchangeNames().empty()) {
    // All public exchanges used
    const auto result = publicExchangePresence.none();
    publicExchangePresence.set();
    return result;
  }
  for (const ExchangeName &exchangeName : command.exchangeNames()) {
    const auto exchangePos = exchangeName.publicExchangePos();
    if (publicExchangePresence[exchangePos]) {
      return false;
    }
    publicExchangePresence.set(exchangePos);
  }
  return true;
}

bool CommandTypeCanBeGrouped(CoincenterCommandType type) {
  // Compatible command types need to be explicitly set
  // For now, only market data is compatible
  switch (type) {
    case CoincenterCommandType::kMarketData:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool CoincenterCommandsIterator::hasNextCommandGroup() const { return _pos < _commands.size(); }

CoincenterCommandsIterator::CoincenterCommandSpan CoincenterCommandsIterator::nextCommandGroup() {
  CoincenterCommandSpan groupedCommands(_commands.begin() + _pos, 1U);

  if (CommandTypeCanBeGrouped(groupedCommands.front().type())) {
    PublicExchangePresenceBitset publicExchangePresence;
    UpdateBitsetAreNewExchanges(groupedCommands.front(), publicExchangePresence);

    while (_pos + groupedCommands.size() < _commands.size()) {
      const CoincenterCommand &nextCommand = _commands[_pos + groupedCommands.size()];
      if (nextCommand.type() != groupedCommands.front().type()) {
        break;
      }
      if (!UpdateBitsetAreNewExchanges(nextCommand, publicExchangePresence)) {
        break;
      }
      // Add new command to group
      groupedCommands = CoincenterCommandSpan(groupedCommands.data(), groupedCommands.size() + 1);
    }
  }

  _pos += groupedCommands.size();
  return groupedCommands;
}

}  // namespace cct