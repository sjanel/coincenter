#include "coincenteroptions.hpp"

#include <cstdlib>
#include <ostream>
#include <string_view>
#include <utility>

#include "cct_config.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "coincentercommandtype.hpp"
#include "curlhandle.hpp"
#include "default-data-dir.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "replay-options.hpp"
#include "ssl_sha.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
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
#ifdef CCT_PROTOBUF_VERSION
  os << "              " << "protobuf " << CCT_PROTOBUF_VERSION << '\n';
#endif
  return os;
}

void CoincenterCmdLineOptions::mergeGlobalWith(const CoincenterCmdLineOptions& rhs) {
  static constexpr CoincenterCmdLineOptions kDefaultOpts;

  // Yes, an ugly macro ! I find it appropriate here, to minimize risk of copy-pasting mistakes.
  // It's also more readable.
#define CCT_OPTIONS_MERGE_GLOBAL_WITH(field) \
  do {                                       \
    if (rhs.field != kDefaultOpts.field) {   \
      (field) = rhs.field;                   \
    }                                        \
  } while (0)

  CCT_OPTIONS_MERGE_GLOBAL_WITH(dataDir);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(logConsole);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(logFile);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(noSecrets);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(repeatTime);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(monitoringAddress);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(monitoringUsername);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(monitoringPassword);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(repeats);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(monitoringPort);
  CCT_OPTIONS_MERGE_GLOBAL_WITH(useMonitoring);

#undef CCT_OPTIONS_MERGE_GLOBAL_WITH
}

TradeOptions CoincenterCmdLineOptions::computeTradeOptions() const {
  const TradeTypePolicy tradeTypePolicy = computeTradeTypePolicy();
  const TradeTimeoutAction timeoutAction = computeTradeTimeoutAction();
  const TradeMode tradeMode = isSimulation ? TradeMode::simulation : TradeMode::real;
  const TradeSyncPolicy tradeSyncPolicy = async ? TradeSyncPolicy::asynchronous : TradeSyncPolicy::synchronous;

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
    return TradeTimeoutAction::cancel;
  }
  if (tradeTimeoutMatch) {
    return TradeTimeoutAction::match;
  }
  return TradeTimeoutAction::exchange_default;
}

WithdrawOptions CoincenterCmdLineOptions::computeWithdrawOptions() const {
  const auto withdrawSyncPolicy = async ? WithdrawSyncPolicy::asynchronous : WithdrawSyncPolicy::synchronous;
  const auto mode = isSimulation ? WithdrawOptions::Mode::kSimulation : WithdrawOptions::Mode::kReal;
  return {withdrawRefreshTime, withdrawSyncPolicy, mode};
}

ReplayOptions CoincenterCmdLineOptions::computeReplayOptions(Duration dur) const {
  if (validate && validateOnly) {
    throw invalid_argument("--validate and --validate-only cannot be specified simultaneously");
  }

  ReplayOptions::ReplayMode replayMode;
  if (validateOnly) {
    replayMode = ReplayOptions::ReplayMode::kValidateOnly;
  } else if (validate) {
    replayMode = ReplayOptions::ReplayMode::kCheckedLaunchAlgorithm;
  } else {
    replayMode = ReplayOptions::ReplayMode::kUncheckedLaunchAlgorithm;
  }

  TimeWindow timeWindow;
  const auto nowTime = Clock::now();
  if (dur == kUndefinedDuration) {
    timeWindow = TimeWindow(TimePoint{}, nowTime);
  } else {
    timeWindow = TimeWindow(nowTime - dur, nowTime);
  }

  return {timeWindow, algorithmNames, replayMode};
}

std::pair<std::string_view, CoincenterCommandType> CoincenterCmdLineOptions::getTradeArgStr() const {
  if (!tradeStrategy.empty() && !tradePrice.empty()) {
    throw invalid_argument("Trade price and trade strategy cannot be set together");
  }
  if (!buy.empty()) {
    return {buy, CoincenterCommandType::Buy};
  }
  if (!sell.empty()) {
    return {sell, CoincenterCommandType::Sell};
  }
  if (!sellAll.empty()) {
    return {sellAll, CoincenterCommandType::Sell};
  }
  if (!tradeAll.empty()) {
    return {tradeAll, CoincenterCommandType::Trade};
  }
  return {trade, CoincenterCommandType::Trade};
}
}  // namespace cct
