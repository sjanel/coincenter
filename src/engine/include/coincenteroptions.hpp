#pragma once

#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include "coincentercommandtype.hpp"
#include "coincenteroptionsdef.hpp"
#include "commandlineoption.hpp"
#include "exchangepublicapi.hpp"
#include "timedef.hpp"
#include "tradedefinitions.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"

namespace cct {

class CoincenterCmdLineOptions {
 public:
  static std::ostream& PrintVersion(std::string_view programName, std::ostream& os) noexcept;

  constexpr CoincenterCmdLineOptions() noexcept = default;

  bool isSmartTrade() const noexcept;

  TradeOptions computeTradeOptions() const;
  WithdrawOptions computeWithdrawOptions() const;

  std::string_view getDataDir() const { return dataDir.empty() ? SelectDefaultDataDir() : dataDir; }

  std::pair<std::string_view, CoincenterCommandType> getTradeArgStr() const;

  void mergeGlobalWith(const CoincenterCmdLineOptions& rhs);

  std::string_view dataDir;

  std::string_view apiOutputType;
  std::string_view logConsole;
  std::string_view logFile;
  std::optional<std::string_view> noSecrets;
  Duration repeatTime = CoincenterCmdLineOptionsDefinitions::kDefaultRepeatTime;

  std::string_view monitoringAddress = CoincenterCmdLineOptionsDefinitions::kDefaultMonitoringIPAddress;
  std::string_view monitoringUsername;
  std::string_view monitoringPassword;

  std::optional<std::string_view> currencies;
  std::optional<std::string_view> markets;

  std::string_view orderbook;
  std::string_view orderbookCur;

  std::optional<std::string_view> healthCheck;

  std::optional<std::string_view> ticker;

  std::string_view conversion;
  std::string_view conversionPath;

  std::optional<std::string_view> balance;

  std::string_view trade;
  std::string_view tradeAll;
  std::string_view tradePrice;
  std::string_view tradeStrategy;
  Duration tradeTimeout = kUndefinedDuration;
  Duration tradeUpdatePrice = kUndefinedDuration;

  std::string_view buy;
  std::string_view sell;
  std::string_view sellAll;

  std::string_view depositInfo;

  std::optional<std::string_view> closedOrdersInfo;
  std::optional<std::string_view> openedOrdersInfo;
  std::optional<std::string_view> cancelOpenedOrders;

  std::optional<std::string_view> recentDepositsInfo;

  std::optional<std::string_view> recentWithdrawsInfo;

  std::string_view ids;
  Duration minAge = kUndefinedDuration;
  Duration maxAge = kUndefinedDuration;

  std::string_view withdrawApply;
  std::string_view withdrawApplyAll;
  std::optional<std::string_view> withdrawFees;
  Duration withdrawRefreshTime{WithdrawOptions().withdrawRefreshTime()};

  std::string_view dustSweeper;

  std::string_view last24hTradedVolume;
  std::string_view lastPrice;

  std::string_view lastTrades;

  CommandLineOptionalInt repeats;
  int nbLastTrades = api::ExchangePublic::kNbLastTradesDefault;
  int monitoringPort = CoincenterCmdLineOptionsDefinitions::kDefaultMonitoringPort;
  int orderbookDepth = 0;

  bool forceMultiTrade = false;
  bool forceSingleTrade = false;
  bool tradeTimeoutMatch = false;
  bool tradeTimeoutCancel = false;
  bool tradeSim{TradeOptions().isSimulation()};
  bool async = false;
  bool help = false;
  bool version = false;
  bool useMonitoring = false;
  bool withBalanceInUse = false;

  bool operator==(const CoincenterCmdLineOptions&) const noexcept = default;

 private:
  static std::string_view SelectDefaultDataDir() noexcept;

  TradeTypePolicy computeTradeTypePolicy() const;
  TradeTimeoutAction computeTradeTimeoutAction() const;
};

}  // namespace cct
