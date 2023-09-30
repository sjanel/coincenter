#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "apioutputtype.hpp"
#include "cct_const.hpp"
#include "commandlineoption.hpp"
#include "exchangeinfomap.hpp"
#include "exchangepublicapi.hpp"
#include "loadconfiguration.hpp"
#include "static_string_view_helpers.hpp"
#include "staticcommandlineoptioncheck.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"

namespace cct {

std::string_view SelectDefaultDataDir() noexcept;

struct CoincenterCmdLineOptions {
  static constexpr std::string_view kDefaultMonitoringIPAddress = "0.0.0.0";  // in Docker, localhost does not work
  static constexpr int kDefaultMonitoringPort = 9091;                         // Prometheus default push port
  static constexpr Duration kDefaultRepeatTime = TimeInS(1);

  static constexpr int64_t kDefaultTradeTimeout =
      std::chrono::duration_cast<TimeInS>(TradeOptions().maxTradeTime()).count();
  static constexpr int64_t kMinUpdatePriceTime =
      std::chrono::duration_cast<TimeInS>(TradeOptions().minTimeBetweenPriceUpdates()).count();
  static constexpr int64_t kDefaultRepeatDurationSeconds =
      std::chrono::duration_cast<TimeInS>(kDefaultRepeatTime).count();

  static constexpr std::string_view kOutput1 = "Output format. One of (";
  static constexpr std::string_view kOutput2 = ") (default configured in general config file)";
  static constexpr std::string_view kOutput =
      JoinStringView_v<kOutput1, kApiOutputTypeNoPrintStr, CharToStringView_v<'|'>, kApiOutputTypeTableStr,
                       CharToStringView_v<'|'>, kApiOutputTypeJsonStr, kOutput2>;

  static constexpr std::string_view kData1 = "Use given 'data' directory instead of the one chosen at build time '";
  static constexpr std::string_view kData = JoinStringView_v<kData1, kDefaultDataDir, CharToStringView_v<'\''>>;

  static constexpr std::string_view kRepeat1 = "Set delay between each repeat (default: ";
  static constexpr std::string_view kRepeat2 = "s)";

  static constexpr std::string_view kRepeat =
      JoinStringView_v<kRepeat1, IntToStringView_v<kDefaultRepeatDurationSeconds>, kRepeat2>;

  static constexpr std::string_view kLastTradesN1 = "Change number of last trades to query (default: ";
  static constexpr std::string_view kLastTradesN =
      JoinStringView_v<kLastTradesN1, IntToStringView_v<api::ExchangePublic::kNbLastTradesDefault>,
                       CharToStringView_v<')'>>;

  static constexpr std::string_view kSmartBuy1 =
      "Attempt to buy the specified amount in total, on matching exchange accounts (all are considered if none "
      "provided)."
      " The base currencies will be chosen according to the '";
  static constexpr std::string_view kSmartBuy2 = "' array defined in '";
  static constexpr std::string_view kSmartBuy3 =
      "' file. "
      "Standard trade options are compatible to customize the trade, and if enabled, multi trade can be used.";
  static constexpr std::string_view kSmartBuy =
      JoinStringView_v<kSmartBuy1, LoadConfiguration::kProdDefaultExchangeConfigFile, kSmartBuy2,
                       kPreferredPaymentCurrenciesOptName, kSmartBuy3>;

  static constexpr std::string_view kSmartSell1 =
      "Attempt to sell the specified amount in total (or percentage with '%'), on matching exchange accounts (all are "
      "considered if none provided)."
      " The payment currencies will be chosen according to the '";
  static constexpr std::string_view kSmartSell =
      JoinStringView_v<kSmartSell1, LoadConfiguration::kProdDefaultExchangeConfigFile, kSmartBuy2,
                       kPreferredPaymentCurrenciesOptName, kSmartBuy3>;

  static constexpr std::string_view kTradeTimeout1 = "Adjust trade timeout (default: ";
  static constexpr std::string_view kTradeTimeout2 = "s). Remaining orders will be cancelled after the timeout.";
  static constexpr std::string_view kTradeTimeout =
      JoinStringView_v<kTradeTimeout1, IntToStringView_v<kDefaultTradeTimeout>, kTradeTimeout2>;

  static constexpr std::string_view kTradeUpdatePrice1 =
      "Set the min time allowed between two limit price updates (default: ";
  static constexpr std::string_view kTradeUpdatePrice2 =
      "s). Avoids cancelling / placing new orders too often with high volumes which can be counter productive "
      "sometimes.";
  static constexpr std::string_view kTradeUpdatePrice =
      JoinStringView_v<kTradeUpdatePrice1, IntToStringView_v<kMinUpdatePriceTime>, kTradeUpdatePrice2>;

  static constexpr std::string_view kSimulationMode1 = "Activates simulation mode only (default: ";
  static constexpr std::string_view kSimulationMode2 = TradeOptions().isSimulation() ? "true" : "false";
  static constexpr std::string_view kSimulationMode3 =
      "). For some exchanges, exchange can be queried in this "
      "mode to validate the trade input options.";
  static constexpr std::string_view kSimulationMode =
      JoinStringView_v<kSimulationMode1, kSimulationMode2, kSimulationMode3>;

  static constexpr std::string_view kWithdraw1 =
      "Withdraw amount from exchange 'from' to exchange 'to'."
      " Amount is gross, including fees, and can be absolute or percentage of all available amount. Address and tag "
      "will be retrieved"
      " automatically from destination exchange and can additionally check if it matches an entry in '";
  static constexpr std::string_view kWithdraw2 = "' file.";
  static constexpr std::string_view kWithdraw = JoinStringView_v<kWithdraw1, kDepositAddressesFileName, kWithdraw2>;

  static constexpr std::string_view kWithdrawRefreshTime1 =
      "Time interval for regular withdraw status checking during synchronous withdrawal. Default is ";
  static constexpr int64_t kDefaultWithdrawRefreshTimeSeconds =
      std::chrono::duration_cast<TimeInS>(WithdrawOptions().withdrawRefreshTime()).count();
  static constexpr std::string_view kWithdrawRefreshTime2 = "s.";
  static constexpr std::string_view kWithdrawRefreshTime =
      JoinStringView_v<kWithdrawRefreshTime1, IntToStringView_v<kDefaultWithdrawRefreshTimeSeconds>,
                       kWithdrawRefreshTime2>;

  static constexpr std::string_view kDustSweeper =
      "Attempts to clean small remaining amount of given currency on each given exchange."
      " The amount is considered 'small' and eligible for dust sweeper process if the 'dustAmountsThreshold' is "
      "set in "
      "the config file for this currency and if starting available amount is lower than this defined threshold."
      " Dust sweeper process is iterative, involving at most 'dustSweeperMaxNbTrades' max trades to be set as well "
      "in "
      "the config file.";

  static constexpr std::string_view kMonitoringPort1 = "Specify port of metric gateway instance (default: ";
  static constexpr std::string_view kMonitoringPort =
      JoinStringView_v<kMonitoringPort1, IntToStringView_v<CoincenterCmdLineOptions::kDefaultMonitoringPort>,
                       CharToStringView_v<')'>>;

  static constexpr std::string_view kMonitoringIP1 = "Specify IP (v4) of metric gateway instance (default: ";
  static constexpr std::string_view kMonitoringIP =
      JoinStringView_v<kMonitoringIP1, CoincenterCmdLineOptions::kDefaultMonitoringIPAddress, CharToStringView_v<')'>>;

  static void PrintVersion(std::string_view programName) noexcept;

  bool isSmartTrade() const noexcept;

  void mergeGlobalWith(const CoincenterCmdLineOptions& other);

  std::string_view dataDir = SelectDefaultDataDir();

  std::string_view apiOutputType;
  std::string_view logConsole;
  std::string_view logFile;
  std::optional<std::string_view> noSecrets;
  Duration repeatTime = kDefaultRepeatTime;

  std::string_view monitoringAddress = kDefaultMonitoringIPAddress;
  std::string_view monitoringUsername;
  std::string_view monitoringPassword;

  std::string_view markets;

  std::string_view orderbook;
  std::string_view orderbookCur;

  std::optional<std::string_view> healthCheck;

  std::optional<std::string_view> ticker;

  std::string_view conversionPath;

  std::optional<std::string_view> balance;

  std::string_view trade;
  std::string_view tradeAll;
  std::string_view tradePrice;
  std::string_view tradeStrategy;
  Duration tradeTimeout{TradeOptions().maxTradeTime()};
  Duration tradeUpdatePrice{TradeOptions().minTimeBetweenPriceUpdates()};

  std::string_view buy;
  std::string_view sell;
  std::string_view sellAll;

  std::string_view depositInfo;

  std::optional<std::string_view> openedOrdersInfo;
  std::optional<std::string_view> cancelOpenedOrders;

  std::optional<std::string_view> recentDepositsInfo;

  std::optional<std::string_view> recentWithdrawsInfo;

  std::string_view ids;
  Duration minAge{};
  Duration maxAge{};

  std::string_view withdraw;
  std::string_view withdrawAll;
  std::string_view withdrawFee;
  Duration withdrawRefreshTime{WithdrawOptions().withdrawRefreshTime()};

  std::string_view dustSweeper;

  std::string_view last24hTradedVolume;
  std::string_view lastPrice;

  std::string_view lastTrades;

  CommandLineOptionalInt repeats;
  int nbLastTrades = api::ExchangePublic::kNbLastTradesDefault;
  int monitoringPort = kDefaultMonitoringPort;
  int orderbookDepth = 0;

  bool forceMultiTrade = false;
  bool forceSingleTrade = false;
  bool tradeTimeoutMatch = false;
  bool tradeSim{TradeOptions().isSimulation()};
  bool tradeAsync = false;  // trade fire and forget mode
  bool help = false;
  bool version = false;
  bool useMonitoring = false;
  bool withBalanceInUse = false;
  bool withdrawAsync = false;  // withdraw fire and forget mode
};

template <class OptValueType>
struct CoincenterAllowedOptions {
  using CommandLineOptionWithValue = AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue;

  static constexpr CommandLineOptionWithValue value[] = {
      {{{"General", 100}, "help", 'h', "", "Display this information"}, &OptValueType::help},
      {{{"General", 200}, "--data", "<path/to/data>", CoincenterCmdLineOptions::kData}, &OptValueType::dataDir},
      {{{"General", 300},
        "--log",
        'v',
        "<levelName|0-6>",
        "Sets the log level in the console during all execution. "
        "Possible values are: (off|critical|error|warning|info|debug|trace) or "
        "(0-6) (overrides .log.console in general config file)"},
       &OptValueType::logConsole},
      {{{"General", 400}, "--log-console", "<levelName|0-6>", "Synonym of --log"}, &OptValueType::logConsole},
      {{{"General", 400},
        "--log-file",
        "<levelName|0-6>",
        "Sets the log level in files during all execution (overrides .log.file in general config file). "
        "Number of rotating files to keep and their size is configurable in the general config file"},
       &OptValueType::logFile},
      {{{"General", 500}, "--output", 'o', "", CoincenterCmdLineOptions::kOutput}, &OptValueType::apiOutputType},
      {{{"General", 700},
        "--no-secrets",
        "<[exch1,...]>",
        "Do not load secrets for specified exchanges.\n"
        "If no exchange is specified, no key will be loaded at all"},
       &OptValueType::noSecrets},
      {{{"General", 800},
        "--repeat",
        'r',
        "<[n]>",
        "Indicates how many repeats to perform for mutable data (such as market data)\n"
        "Modifying requests such as trades and withdraws are not impacted by this option. "
        "This is useful for monitoring for instance. 'n' is optional, if not given, will repeat endlessly"},
       &OptValueType::repeats},
      {{{"General", 900}, "--repeat-time", "<time>", CoincenterCmdLineOptions::kRepeat}, &OptValueType::repeatTime},
      {{{"General", 1000}, "version", "", "Display program version"}, &OptValueType::version},
      {{{"Public queries", 2000},
        "health-check",
        "<[exch1,...]>",
        "Simple health check for all exchanges or specified ones"},
       &OptValueType::healthCheck},
      {{{"Public queries", 2100},
        "markets",
        "<cur1[-cur2][,exch1,...]>",
        "Print markets involving given currencies for all exchanges, "
        "or only the specified ones. "
        "Either a single currency or a full market can be specified."},
       &OptValueType::markets},

      {{{"Public queries", 2200},
        "orderbook",
        "<cur1-cur2[,exch1,...]>",
        "Print order book of currency pair for all exchanges offering "
        "this market, or only for specified exchanges."},
       &OptValueType::orderbook},
      {{{"Public queries", 2300},
        "--cur",
        "<cur>",
        "If conversion of cur2 into cur is possible (for each exchange), "
        "prints additional column converted to given asset"},
       &OptValueType::orderbookCur},
      {{{"Public queries", 2300}, "--depth", "<n>", "Override default depth of order book"},
       &OptValueType::orderbookDepth},
      {{{"Public queries", 2400},
        "ticker",
        "<[exch1,...]>",
        "Print ticker information for all markets for all exchanges,"
        " or only for specified ones"},
       &OptValueType::ticker},
      {{{"Public queries", 2500},
        "conversion",
        "<cur1-cur2[,exch1,...]>",
        "Print fastest conversion path of 'cur1' to 'cur2' "
        "for given exchanges if possible"},
       &OptValueType::conversionPath},
      {{{"Public queries", 2600},
        "volume-day",
        "<cur1-cur2[,exch1,...]>",
        "Print last 24h traded volume for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::last24hTradedVolume},
      {{{"Public queries", 2700},
        "last-trades",
        "<cur1-cur2[,exch1,...]>",
        "Print last trades for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::lastTrades},
      {{{"Public queries", 2800}, "--n", "<n>", CoincenterCmdLineOptions::kLastTradesN}, &OptValueType::nbLastTrades},
      {{{"Public queries", 2900},
        "price",
        "<cur1-cur2[,exch1,...]>",
        "Print last price for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::lastPrice},

      {{{"Private queries", 3000},
        "balance",
        "<[cur][,exch1,...]>",
        "Prints sum of available balance for all selected accounts, "
        "or all if none given. Optional acronym can be provided, "
        "in this case a total amount will be printed in this currency "
        "if conversion is possible."},
       &OptValueType::balance},
      {{{"Private queries", 3100}, "--in-use", "", "Include balance in use as well from opened orders"},
       &OptValueType::withBalanceInUse},
      {{{"Private queries", 3250},
        "orders-opened",
        "<cur1-cur2[,exch1,...]>",
        "Print opened orders matching selection criteria.\n"
        "All cur1, cur2 and exchanges are optional, "
        "returned opened orders will be filtered accordingly."},
       &OptValueType::openedOrdersInfo},
      {{{"Private queries", 3260},
        "orders-cancel",
        "<cur1-cur2[,exch1,...]>",
        "Cancel opened orders matching selection criteria.\n"
        "All cur1, cur2 and exchanges are optional."},
       &OptValueType::cancelOpenedOrders},
      {{{"Private queries", 3300},
        "--id",
        "<id1,...>",
        "Only select orders with given ID.\n"
        "One or several IDs can be given, should be comma separated."},
       &OptValueType::ids},
      {{{"Private queries", 3301}, "--min-age", "<time>", "Only select orders with given minimum age."},
       &OptValueType::minAge},
      {{{"Private queries", 3302}, "--max-age", "<time>", "Only select orders with given maximum age."},
       &OptValueType::maxAge},
      {{{"Private queries", 3400},
        "deposits",
        "<cur[,exch1,...]>",
        "Print recent deposits matching selection criteria.\n"
        "Currency and exchanges are optional, "
        "returned deposits will be filtered accordingly."},
       &OptValueType::recentDepositsInfo},
      {{{"Private queries", 3600},
        "--id",
        "<id1,...>",
        "Only select deposits with given ID.\n"
        "One or several IDs can be given, should be comma separated."},
       &OptValueType::ids},
      {{{"Private queries", 3601}, "--min-age", "<time>", "Only select deposits with given minimum age."},
       &OptValueType::minAge},
      {{{"Private queries", 3602}, "--max-age", "<time>", "Only select deposits with given maximum age."},
       &OptValueType::maxAge},
      {{{"Private queries", 3800},
        "withdraws",
        "<cur[,exch1,...]>",
        "Print recent withdraws matching selection criteria.\n"
        "Currency and exchanges are optional, "
        "returned withdraws will be filtered accordingly."},
       &OptValueType::recentWithdrawsInfo},
      {{{"Private queries", 3900},
        "--id",
        "<id1,...>",
        "Only select withdraws with given ID.\n"
        "One or several IDs can be given, should be comma separated."},
       &OptValueType::ids},
      {{{"Private queries", 3901}, "--min-age", "<time>", "Only select withdraws with given minimum age."},
       &OptValueType::minAge},
      {{{"Private queries", 3902}, "--max-age", "<time>", "Only select withdraws with given maximum age."},
       &OptValueType::maxAge},
      {{{"Trade", 4000}, "buy", "<amt cur[,exch1,...]>", CoincenterCmdLineOptions::kSmartBuy}, &OptValueType::buy},
      {{{"Trade", 4000}, "sell", "<amt[%]cur[,exch1,...]>", CoincenterCmdLineOptions::kSmartSell}, &OptValueType::sell},
      {{{"Trade", 4000},
        "sell-all",
        "<cur[,exch1,...]>",
        "Sell all available amount on matching exchanges (or all if none specified), behaving like sell option."},
       &OptValueType::sellAll},
      {{{"Trade", 4000},
        "trade",
        "<amt[%]cur1-cur2[,exch1,...]>",
        "Single trade from given start amount on a list of exchanges, "
        "or all that have sufficient balance on cur1 if none provided.\n"
        "Amount can be given as a percentage - in this case the desired percentage "
        "of available amount on matching exchanges will be traded.\n"
        "Orders will be placed prioritizing accounts with largest amounts, at limit price by default."},
       &OptValueType::trade},
      {{{"Trade", 4000},
        "trade-all",
        "<cur1-cur2[,exch1,...]>",
        "Single trade from available amount from given currency on a list of exchanges,"
        " or all that have some balance on cur1 if none provided\n"
        "Order will be placed at limit price by default"},
       &OptValueType::tradeAll},
      {{{"Trade", 4010},
        "--async",
        "",
        "Asynchronous trade mode. Trade orders will be sent in fire and forget mode, not following their lifetime "
        "until either match or cancel occurs.\nThis option is not compatible with multi trade."},
       &OptValueType::tradeAsync},
      {{{"Trade", 4010},
        "--multi-trade",
        "",
        "Force activation of multi trade mode for all exchanges, overriding default configuration of config file.\n"
        "It makes trade in multiple steps possible if exchange does not provide a direct currency market pair.\n"
        "The conversion path used is always one of the fastest(s). All other trade options apply to one unique trade "
        "step (for instance, the trade timeout is related to a single trade, not the series of all trades of a multi "
        "trade)."},
       &OptValueType::forceMultiTrade},
      {{{"Trade", 4020},
        "--no-multi-trade",
        "",
        "Force deactivation of multi trade mode for all exchanges, overriding default configuration of config file."},
       &OptValueType::forceSingleTrade},
      {{{"Trade", 4030},
        "--price",
        "<n|amt cur>",
        "Manually select trade price, supporting two flavors.\n"
        "  'n'      : price will be chosen according to the 'n'th price\n"
        "             of the order book (0: limit price)\n"
        "  'amt cur': price will be fixed at given price\n"
        "             Order price will not be continuously updated.\n"
        "This option is not compatible with '--strategy'"},
       &OptValueType::tradePrice},
      {{{"Trade", 4030},
        "--strategy",
        "<maker|nibble|taker>",
        "Customize the order price strategy of the trade\n"
        "  'maker' : order price set at limit price (default)\n"
        "  'nibble': order price set at limit price +(buy)/-(sell) 1\n"
        "  'taker' : order price will be at market price and matched immediately\n"
        "Order price will be continuously updated and recomputed every '--update-price' step time.\n"
        "This option is not compatible with '--price'"},
       &OptValueType::tradeStrategy},
      {{{"Trade", 4030}, "--timeout", "<time>", CoincenterCmdLineOptions::kTradeTimeout}, &OptValueType::tradeTimeout},
      {{{"Trade", 4030},
        "--timeout-match",
        "",
        "If after the timeout some amount is still not traded,\n"
        "force match by placing a remaining order at market price"},
       &OptValueType::tradeTimeoutMatch},
      {{{"Trade", 4030}, "--update-price", "<time>", CoincenterCmdLineOptions::kTradeUpdatePrice},
       &OptValueType::tradeUpdatePrice},
      {{{"Trade", 4030}, "--sim", "", CoincenterCmdLineOptions::kSimulationMode}, &OptValueType::tradeSim},
      {{{"Tools", 5000}, "dust-sweeper", "<cur[,exch1,...]>", CoincenterCmdLineOptions::kDustSweeper},
       &OptValueType::dustSweeper},
      {{{"Withdraw and deposit", 6000},
        "deposit-info",
        "<cur[,exch1,...]>",
        "Get deposit wallet information for given currency."
        " If no exchange accounts are given, will query all of them by default"},
       &OptValueType::depositInfo},
      {{{"Withdraw and deposit", 6000}, "withdraw-apply", "<amt[%]cur,from-to>", CoincenterCmdLineOptions::kWithdraw},
       &OptValueType::withdraw},
      {{{"Withdraw and deposit", 6010},
        "--async",
        "",
        "Initiate withdraw but do not wait for funds' arrival at destination."},
       &OptValueType::withdrawAsync},
      {{{"Withdraw and deposit", 6010}, "--refresh-time", "<time>", CoincenterCmdLineOptions::kWithdrawRefreshTime},
       &OptValueType::withdrawRefreshTime},
      {{{"Withdraw and deposit", 7000},
        "withdraw-apply-all",
        "<cur,from-to>",
        "Withdraw all available amount instead of a specified amount."},
       &OptValueType::withdrawAll},
      {{{"Withdraw and deposit", 8000},
        "withdraw-fee",
        "<cur[,exch1,...]>",
        "Prints withdraw fees of given currency on all supported exchanges,"
        " or only for the list of specified ones if provided (comma separated)."},
       &OptValueType::withdrawFee},
      {{{"Monitoring", 9000},
        "--monitoring",
        "",
        "Progressively send metrics to external instance provided that it's correctly set up "
        "(Prometheus by default). Refer to the README for more information"},
       &OptValueType::useMonitoring},
      {{{"Monitoring", 9000}, "--monitoring-port", "<port>", CoincenterCmdLineOptions::kMonitoringPort},
       &OptValueType::monitoringPort},
      {{{"Monitoring", 9000}, "--monitoring-ip", "<IPv4>", CoincenterCmdLineOptions::kMonitoringIP},
       &OptValueType::monitoringAddress},
      {{{"Monitoring", 9000},
        "--monitoring-user",
        "<username>",
        "Specify username of metric gateway instance (default: none)"},
       &OptValueType::monitoringUsername},
      {{{"Monitoring", 9000},
        "--monitoring-pass",
        "<password>",
        "Specify password of metric gateway instance (default: none)"},
       &OptValueType::monitoringPassword}};

  static_assert(StaticCommandLineOptionsDuplicatesCheck(std::to_array(value)),
                "Duplicated option names (short hand flag / long name)");
  static_assert(StaticCommandLineOptionsDescriptionCheck(std::to_array(value)),
                "Description of a command line option should not start nor end with a '\n' or space");
};

}  // namespace cct
