#include "coincenteroptions.hpp"

#include <cstdlib>
#include <ostream>
#include <string_view>
#include <utility>

#include "cct_config.hpp"
#include "cct_const.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "coincentercommandtype.hpp"
#include "curlhandle.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "ssl_sha.hpp"
#include "tradedefinitions.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"

namespace cct {

std::string_view CoincenterCmdLineOptions::SelectDefaultDataDir() noexcept {
  const char* pDataDirEnvValue = std::getenv("CCT_DATA_DIR");
  if (pDataDirEnvValue != nullptr) {
    return pDataDirEnvValue;
  }
  return kDefaultDataDir;
}

bool CoincenterCmdLineOptions::isSmartTrade() const noexcept {
  return !buy.empty() || !sell.empty() || !sellAll.empty();
}

std::ostream& CoincenterCmdLineOptions::PrintVersion(std::string_view programName, std::ostream& os) noexcept {
  os << programName << " version " << CCT_VERSION << '\n';
  os << "compiled with " << CCT_COMPILER_VERSION << " on " << __DATE__ << " at " << __TIME__ << '\n';
  os << "              " << GetCurlVersionInfo() << '\n';
  os << "              " << ssl::GetOpenSSLVersion() << '\n';
  return os;
}

void CoincenterCmdLineOptions::mergeGlobalWith(const CoincenterCmdLineOptions& rhs) {
  static constexpr CoincenterCmdLineOptions kDefaultOpts;

  if (rhs.dataDir != kDefaultOpts.dataDir) {
    dataDir = rhs.dataDir;
  }
  if (rhs.logConsole != kDefaultOpts.logConsole) {
    logConsole = rhs.logConsole;
  }
  if (rhs.logFile != kDefaultOpts.logFile) {
    logFile = rhs.logFile;
  }
  if (rhs.noSecrets != kDefaultOpts.noSecrets) {
    noSecrets = rhs.noSecrets;
  }
  if (rhs.repeatTime != kDefaultOpts.repeatTime) {
    repeatTime = rhs.repeatTime;
  }
  if (rhs.monitoringAddress != kDefaultOpts.monitoringAddress) {
    monitoringAddress = rhs.monitoringAddress;
  }
  if (rhs.monitoringUsername != kDefaultOpts.monitoringUsername) {
    monitoringUsername = rhs.monitoringUsername;
  }
  if (rhs.monitoringPassword != kDefaultOpts.monitoringPassword) {
    monitoringPassword = rhs.monitoringPassword;
  }

  if (rhs.repeats != kDefaultOpts.repeats) {
    repeats = rhs.repeats;
  }

  if (rhs.monitoringPort != kDefaultOpts.monitoringPort) {
    monitoringPort = rhs.monitoringPort;
  }
  if (rhs.useMonitoring != kDefaultOpts.useMonitoring) {
    useMonitoring = rhs.useMonitoring;
  }
}

TradeOptions CoincenterCmdLineOptions::computeTradeOptions() const {
  const TradeTypePolicy tradeTypePolicy = computeTradeTypePolicy();
  const TradeTimeoutAction timeoutAction = computeTradeTimeoutAction();
  const TradeMode tradeMode = isSimulation ? TradeMode::kSimulation : TradeMode::kReal;
  const TradeSyncPolicy tradeSyncPolicy = async ? TradeSyncPolicy::kAsynchronous : TradeSyncPolicy::kSynchronous;

  if (!tradeStrategy.empty()) {
    PriceOptions priceOptions(tradeStrategy);
    return {priceOptions, timeoutAction, tradeMode, tradeTimeout, tradeUpdatePrice, tradeTypePolicy, tradeSyncPolicy};
  }
  if (!tradePrice.empty()) {
    MonetaryAmount tradePriceAmount(tradePrice);
    if (tradePriceAmount.isAmountInteger() && tradePriceAmount.hasNeutralCurrency()) {
      // Then it must be a relative price
      RelativePrice relativePrice = static_cast<RelativePrice>(tradePriceAmount.integerPart());
      PriceOptions priceOptions(relativePrice);
      return {priceOptions, timeoutAction, tradeMode, tradeTimeout, tradeUpdatePrice, tradeTypePolicy, tradeSyncPolicy};
    }
    if (isSmartTrade()) {
      throw invalid_argument("Absolute price is not compatible with smart buy / sell");
    }
    // fixed price
    PriceOptions priceOptions(tradePriceAmount);
    return {priceOptions, timeoutAction, tradeMode, tradeTimeout, tradeUpdatePrice, tradeTypePolicy, tradeSyncPolicy};
  }
  // Default - use exchange config file values
  return {PriceOptions{}, timeoutAction, tradeMode, tradeTimeout, tradeUpdatePrice, tradeTypePolicy, tradeSyncPolicy};
}

TradeTypePolicy CoincenterCmdLineOptions::computeTradeTypePolicy() const {
  if (forceMultiTrade) {
    if (forceSingleTrade) {
      throw invalid_argument("Multi & Single trade cannot be forced at the same time");
    }
    if (async) {
      throw invalid_argument("Cannot use force multi trade and asynchronous mode at the same time");
    }
    return TradeTypePolicy::kForceMultiTrade;
  }
  if (forceSingleTrade || async) {
    return TradeTypePolicy::kForceSingleTrade;
  }

  return TradeTypePolicy::kDefault;
}

TradeTimeoutAction CoincenterCmdLineOptions::computeTradeTimeoutAction() const {
  if (tradeTimeoutCancel) {
    if (tradeTimeoutMatch) {
      throw invalid_argument("Only one trade timeout action may be chosen");
    }
    return TradeTimeoutAction::kCancel;
  }
  if (tradeTimeoutMatch) {
    return TradeTimeoutAction::kMatch;
  }
  return TradeTimeoutAction::kDefault;
}

WithdrawOptions CoincenterCmdLineOptions::computeWithdrawOptions() const {
  const auto withdrawSyncPolicy = async ? WithdrawSyncPolicy::kAsynchronous : WithdrawSyncPolicy::kSynchronous;
  const auto mode = isSimulation ? WithdrawOptions::Mode::kSimulation : WithdrawOptions::Mode::kReal;
  return {withdrawRefreshTime, withdrawSyncPolicy, mode};
}

std::pair<std::string_view, CoincenterCommandType> CoincenterCmdLineOptions::getTradeArgStr() const {
  if (!tradeStrategy.empty() && !tradePrice.empty()) {
    throw invalid_argument("Trade price and trade strategy cannot be set together");
  }
  if (!buy.empty()) {
    return {buy, CoincenterCommandType::kBuy};
  }
  if (!sell.empty()) {
    return {sell, CoincenterCommandType::kSell};
  }
  if (!sellAll.empty()) {
    return {sellAll, CoincenterCommandType::kSell};
  }
  if (!tradeAll.empty()) {
    return {tradeAll, CoincenterCommandType::kTrade};
  }
  return {trade, CoincenterCommandType::kTrade};
}
}  // namespace cct
