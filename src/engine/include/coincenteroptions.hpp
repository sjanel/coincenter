#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_const.hpp"
#include "commandlineoption.hpp"
#include "exchangeinfomap.hpp"
#include "exchangepublicapi.hpp"
#include "static_string_view_helpers.hpp"
#include "staticcommandlineoptioncheck.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"
#include "tradeoptions.hpp"

namespace cct {

struct CoincenterCmdLineOptions {
  static constexpr std::string_view kDefaultMonitoringIPAddress = "0.0.0.0";  // in Docker, localhost does not work
  static constexpr int kDefaultMonitoringPort = 9091;                         // Prometheus default push port
  static constexpr Duration kDefaultRepeatTime = std::chrono::seconds(1);

  static constexpr int64_t kDefaultTradeTimeout =
      std::chrono::duration_cast<std::chrono::seconds>(TradeOptions().maxTradeTime()).count();
  static constexpr int64_t kMinUpdatePriceTime =
      std::chrono::duration_cast<std::chrono::seconds>(TradeOptions().minTimeBetweenPriceUpdates()).count();
  static constexpr int64_t kDefaultRepeatDurationSeconds =
      std::chrono::duration_cast<std::chrono::seconds>(kDefaultRepeatTime).count();

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
      " automatically from destination exchange and should match an entry in '";
  static constexpr std::string_view kWithdraw2 = "' file.";
  static constexpr std::string_view kWithdraw = JoinStringView_v<kWithdraw1, kDepositAddressesFileName, kWithdraw2>;

  static constexpr std::string_view kMonitoringPort1 = "Specify port of metric gateway instance (default: ";
  static constexpr std::string_view kMonitoringPort =
      JoinStringView_v<kMonitoringPort1, IntToStringView_v<CoincenterCmdLineOptions::kDefaultMonitoringPort>,
                       CharToStringView_v<')'>>;

  static constexpr std::string_view kMonitoringIP1 = "Specify IP (v4) of metric gateway instance (default: ";
  static constexpr std::string_view kMonitoringIP =
      JoinStringView_v<kMonitoringIP1, CoincenterCmdLineOptions::kDefaultMonitoringIPAddress, CharToStringView_v<')'>>;

  static void PrintVersion(std::string_view programName);

  std::string_view dataDir = kDefaultDataDir;

  std::string_view apiOutputType;
  std::string_view logLevel;
  bool help = false;
  bool version = false;
  bool logFile = false;
  bool logConsole = false;
  std::optional<std::string_view> nosecrets;
  CommandLineOptionalInt repeats;
  Duration repeatTime = kDefaultRepeatTime;

  std::string_view monitoringAddress = kDefaultMonitoringIPAddress;
  std::string_view monitoringUsername;
  std::string_view monitoringPassword;
  int monitoringPort = kDefaultMonitoringPort;
  bool useMonitoring = false;

  std::string_view markets;

  std::string_view orderbook;
  int orderbookDepth = 0;
  std::string_view orderbookCur;

  std::optional<std::string_view> ticker;

  std::string_view conversionPath;

  std::optional<std::string_view> balance;

  std::string_view trade;
  std::string_view tradeAll;
  std::string_view tradePrice;
  std::string_view tradeStrategy;
  Duration tradeTimeout{TradeOptions().maxTradeTime()};
  Duration tradeUpdatePrice{TradeOptions().minTimeBetweenPriceUpdates()};

  bool forceMultiTrade = false;
  bool forceSingleTrade = false;
  bool tradeTimeoutMatch = false;
  bool tradeSim{TradeOptions().isSimulation()};

  std::string_view buy;
  std::string_view sell;
  std::string_view sellAll;

  std::string_view depositInfo;

  std::optional<std::string_view> openedOrdersInfo;
  std::optional<std::string_view> cancelOpenedOrders;
  std::string_view ordersIds;
  Duration ordersMinAge{};
  Duration ordersMaxAge{};

  std::string_view withdraw;
  std::string_view withdrawAll;
  std::string_view withdrawFee;

  std::string_view last24hTradedVolume;
  std::string_view lastPrice;

  std::string_view lastTrades;
  int nbLastTrades = api::ExchangePublic::kNbLastTradesDefault;
};

template <class OptValueType>
struct CoincenterAllowedOptions {
  // TODO: Once clang implements P0634R3, remove 'typename' here
  using CommandLineOptionWithValue = typename AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue;

  static constexpr CommandLineOptionWithValue value[] = {
      {{{"General", 1}, "--help", 'h', "", "Display this information"}, &OptValueType::help},
      {{{"General", 2}, "--data", 'd', "<path/to/data>", CoincenterCmdLineOptions::kData}, &OptValueType::dataDir},
      {{{"General", 3},
        "--log",
        'v',
        "<levelname|0-6>",
        "Sets the log level during all execution. "
        "Possible values are: \n(off|critical|error|warning|info|debug|trace) or "
        "(0-6) (default configured in general config file)"},
       &OptValueType::logLevel},
      {{{"General", 4}, "--log-console", "", "Log to stdout (default configured in general config file)"},
       &OptValueType::logConsole},
      {{{"General", 4}, "--log-file", "", "Log to rotating files (default configured in general config file)"},
       &OptValueType::logFile},
      {{{"General", 5}, "--output", 'o', "", CoincenterCmdLineOptions::kOutput}, &OptValueType::apiOutputType},
      {{{"General", 7},
        "--no-secrets",
        "<[exch1,...]>",
        "Even if present, do not load secrets and do not use private exchanges.\n"
        "If empty list of exchanges, it skips secrets load for all private exchanges"},
       &OptValueType::nosecrets},
      {{{"General", 8},
        "--repeat",
        'r',
        "<[n]>",
        "Indicates how many repeats to perform for mutable data (such as market data)\n"
        "Modifying requests such as trades and withdraws are not impacted by this option. "
        "This is useful for monitoring for instance. 'n' is optional, if not given, will repeat endlessly"},
       &OptValueType::repeats},
      {{{"General", 9}, "--repeat-time", "<time>", CoincenterCmdLineOptions::kRepeat}, &OptValueType::repeatTime},
      {{{"General", 10}, "--version", "", "Display program version"}, &OptValueType::version},
      {{{"Public queries", 20},
        "--markets",
        'm',
        "<cur1[-cur2][,exch1,...]>",
        "Print markets involving given currencies for all exchanges, "
        "or only the specified ones. "
        "Either a single currency or a full market can be specified."},
       &OptValueType::markets},

      {{{"Public queries", 20},
        "--orderbook",
        "<cur1-cur2[,exch1,...]>",
        "Print order book of currency pair for all exchanges offering "
        "this market, or only for specified exchanges."},
       &OptValueType::orderbook},
      {{{"Public queries", 20}, "--orderbook-depth", "<n>", "Override default depth of order book"},
       &OptValueType::orderbookDepth},
      {{{"Public queries", 20},
        "--orderbook-cur",
        "<cur>",
        "If conversion of cur2 into cur is possible (for each exchange), "
        "prints additional column converted to given asset"},
       &OptValueType::orderbookCur},
      {{{"Public queries", 20},
        "--ticker",
        "<[exch1,...]>",
        "Print ticker information for all markets for all exchanges,"
        " or only for specified ones"},
       &OptValueType::ticker},
      {{{"Public queries", 20},
        "--conversion",
        'c',
        "<cur1-cur2[,exch1,...]>",
        "Print fastest conversion path of 'cur1' to 'cur2' "
        "for given exchanges if possible"},
       &OptValueType::conversionPath},
      {{{"Public queries", 20},
        "--volume-day",
        "<cur1-cur2[,exch1,...]>",
        "Print last 24h traded volume for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::last24hTradedVolume},
      {{{"Public queries", 20},
        "--last-trades",
        "<cur1-cur2[,exch1,...]>",
        "Print last trades for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::lastTrades},
      {{{"Public queries", 20}, "--last-trades-n", "<n>", CoincenterCmdLineOptions::kLastTradesN},
       &OptValueType::nbLastTrades},
      {{{"Public queries", 20},
        "--price",
        'p',
        "<cur1-cur2[,exch1,...]>",
        "Print last price for market 'cur1'-'cur2' "
        "for all exchanges (or specified one)"},
       &OptValueType::lastPrice},

      {{{"Private queries", 30},
        "--balance",
        'b',
        "<[cur][,exch1,...]>",
        "Prints sum of available balance for all selected accounts, "
        "or all if none given. Optional acronym can be provided, "
        "in this case a total amount will be printed in this currency "
        "if conversion is possible."},
       &OptValueType::balance},
      {{{"Private queries", 31},
        "--orders-opened",
        "<cur1-cur2[,exch1,...]>",
        "Print opened orders matching selection criteria.\n"
        "All cur1, cur2 and exchanges are optional, "
        "returned opened orders will be filtered accordingly."},
       &OptValueType::openedOrdersInfo},
      {{{"Private queries", 32},
        "--orders-cancel",
        "<cur1-cur2[,exch1,...]>",
        "Cancel opened orders matching selection criteria.\n"
        "All cur1, cur2 and exchanges are optional."},
       &OptValueType::cancelOpenedOrders},
      {{{"Private queries", 34},
        "--orders-id",
        "<id1,...>",
        "Only select orders with given ID.\n"
        "One or several IDs can be given, should be comma separated."},
       &OptValueType::ordersIds},
      {{{"Private queries", 35}, "--orders-min-age", "<time>", "Only select orders with given minimum age."},
       &OptValueType::ordersMinAge},
      {{{"Private queries", 36}, "--orders-max-age", "<time>", "Only select orders with given maximum age."},
       &OptValueType::ordersMaxAge},
      {{{"Trade", 40}, "--buy", "<amt cur[,exch1,...]>", CoincenterCmdLineOptions::kSmartBuy}, &OptValueType::buy},
      {{{"Trade", 40}, "--sell", "<amt[%]cur[,exch1,...]>", CoincenterCmdLineOptions::kSmartSell}, &OptValueType::sell},
      {{{"Trade", 40},
        "--sell-all",
        "<cur[,exch1,...]>",
        "Sell all available amount on matching exchanges (or all if none specified), behaving like sell option."},
       &OptValueType::sellAll},
      {{{"Trade", 40},
        "--trade",
        't',
        "<amt[%]cur1-cur2[,exch1,...]>",
        "Single trade from given start amount on a list of exchanges, "
        "or all that have sufficient balance on cur1 if none provided.\n"
        "Amount can be given as a percentage - in this case the desired percentage "
        "of available amount on matching exchanges will be traded.\n"
        "Orders will be placed prioritizing accounts with largest amounts, at limit price by default."},
       &OptValueType::trade},
      {{{"Trade", 40},
        "--trade-all",
        "<cur1-cur2[,exch1,...]>",
        "Single trade from available amount from given currency on a list of exchanges,"
        " or all that have some balance on cur1 if none provided\n"
        "Order will be placed at limit price by default"},
       &OptValueType::tradeAll},
      {{{"Trade", 41},
        "--multi-trade",
        "",
        "Force activation of multi trade mode for all exchanges, overriding default configuration of config file.\n"
        "It makes trade in multiple steps possible if exchange does not provide a direct currency market pair.\n"
        "The conversion path used is always one of the fastest(s). All other trade options apply to one unique trade "
        "step (for instance, the trade timeout is related to a single trade, not the series of all trades of a multi "
        "trade)."},
       &OptValueType::forceMultiTrade},
      {{{"Trade", 42},
        "--no-multi-trade",
        "",
        "Force deactivation of multi trade mode for all exchanges, overriding default configuration of config file."},
       &OptValueType::forceSingleTrade},
      {{{"Trade", 43},
        "--trade-price",
        "<n|amt cur>",
        "Manually select trade price, supporting two flavors.\n"
        "  'n'      : price will be chosen according to the 'n'th price\n"
        "             of the order book (0: limit price)\n"
        "  'amt cur': price will be fixed at given price\n"
        "             Order price will not be continuously updated.\n"
        "This option is not compatible with '--trade-strategy'"},
       &OptValueType::tradePrice},
      {{{"Trade", 43},
        "--trade-strategy",
        "<maker|nibble|taker>",
        "Customize the order price strategy of the trade\n"
        "  'maker' : order price set at limit price (default)\n"
        "  'nibble': order price set at limit price +(buy)/-(sell) 1\n"
        "  'taker' : order price will be at market price and matched immediately\n"
        "Order price will be continuously updated and recomputed every '--trade-updateprice' step time.\n"
        "This option is not compatible with '--trade-price'"},
       &OptValueType::tradeStrategy},
      {{{"Trade", 43}, "--trade-timeout", "<time>", CoincenterCmdLineOptions::kTradeTimeout},
       &OptValueType::tradeTimeout},
      {{{"Trade", 43},
        "--trade-timeout-match",
        "",
        "If after the timeout some amount is still not traded,\n"
        "force match by placing a remaining order at market price\n"},
       &OptValueType::tradeTimeoutMatch},
      {{{"Trade", 43}, "--trade-updateprice", "<time>", CoincenterCmdLineOptions::kTradeUpdatePrice},
       &OptValueType::tradeUpdatePrice},
      {{{"Trade", 44}, "--trade-sim", "", CoincenterCmdLineOptions::kSimulationMode}, &OptValueType::tradeSim},
      {{{"Withdraw and deposit", 50},
        "--deposit-info",
        "<cur[,exch1,...]>",
        "Get deposit wallet information for given currency."
        " If no exchange accounts are given, will query all of them by default"},
       &OptValueType::depositInfo},
      {{{"Withdraw and deposit", 50}, "--withdraw", 'w', "<amt[%]cur,from-to>", CoincenterCmdLineOptions::kWithdraw},
       &OptValueType::withdraw},
      {{{"Withdraw and deposit", 50},
        "--withdraw-all",
        "<cur,from-to>",
        "Withdraw all available amount instead of a specified amount."},
       &OptValueType::withdrawAll},
      {{{"Withdraw and deposit", 50},
        "--withdraw-fee",
        "<cur[,exch1,...]>",
        "Prints withdraw fees of given currency on all supported exchanges,"
        " or only for the list of specified ones if provided (comma separated)."},
       &OptValueType::withdrawFee},
      {{{"Monitoring", 60},
        "--monitoring",
        "",
        "Progressively send metrics to external instance provided that it's correctly set up "
        "(Prometheus by default). Refer to the README for more information"},
       &OptValueType::useMonitoring},
      {{{"Monitoring", 60}, "--monitoring-port", "<port>", CoincenterCmdLineOptions::kMonitoringPort},
       &OptValueType::monitoringPort},
      {{{"Monitoring", 60}, "--monitoring-ip", "<IPv4>", CoincenterCmdLineOptions::kMonitoringIP},
       &OptValueType::monitoringAddress},
      {{{"Monitoring", 60},
        "--monitoring-user",
        "<username>",
        "Specify username of metric gateway instance (default: none)"},
       &OptValueType::monitoringUsername},
      {{{"Monitoring", 60},
        "--monitoring-pass",
        "<password>",
        "Specify password of metric gateway instance (default: none)"},
       &OptValueType::monitoringPassword}};

  static_assert(StaticCommandLineOptionsCheck(std::to_array(value)),
                "Duplicated option names (short hand flag / long name)");
};

}  // namespace cct
