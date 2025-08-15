#pragma once

#include <optional>
#include <span>
#include <utility>

#include "cct_fixedcapacityvector.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "duration-schema.hpp"
#include "exchange-name-enum.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "time-window.hpp"
#include "timepoint-schema.hpp"
#include "tradedamounts.hpp"
#include "withdrawoptions.hpp"

namespace cct::schema::queryresult {

struct HealthCheck {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::HealthCheck;
  } in;
  FixedCapacityVector<std::pair<ExchangeNameEnum, bool>, kNbSupportedExchanges> out;
};

struct CurrenciesPerExchange {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Currencies;
  } in;

  struct Currency {
    CurrencyCode code;
    CurrencyCode exchangeCode;
    CurrencyCode altCode;
    bool canDeposit;
    bool canWithdraw;
    bool isFiat;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, vector<Currency>>, kNbSupportedExchanges> out;
};

struct Markets {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Markets;
    struct Opt {
      std::optional<CurrencyCode> cur1;
      std::optional<CurrencyCode> cur2;
    } opt;
  } in;

  FixedCapacityVector<std::pair<ExchangeNameEnum, const MarketSet &>, kNbSupportedExchanges> out;
};

struct MarketsForReplay {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::ReplayMarkets;
    struct Opt {
      std::optional<TimeWindow> timeWindow;
    } opt;
  } in;

  struct ExchangePart {
    struct Elem {
      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Elem &) const = default;

      Market market;
      string lastTimestamp;
    };

    vector<Elem> orderBooks;
    vector<Elem> trades;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct TickerInformation {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Ticker;
  } in;

  struct Ticker {
    Market pair;

    struct Elem {
      MonetaryAmount a;
      MonetaryAmount p;
    };

    Elem ask;
    Elem bid;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, vector<Ticker>>, kNbSupportedExchanges> out;
};

struct MarketOrderBooks {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Orderbook;
    struct Opt {
      Market pair;
      std::optional<CurrencyCode> equiCurrency;
      std::optional<int> depth;
    } opt;
  } in;

  struct ExchangePart {
    struct AskOrBid {
      auto operator<=>(const AskOrBid &) const = default;

      MonetaryAmount a;
      MonetaryAmount p;
      std::optional<MonetaryAmount> eq;
    };

    vector<AskOrBid> ask;
    vector<AskOrBid> bid;
    TimePointIso8601UTC time;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct Balance {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Balance;
    struct Opt {
      std::optional<CurrencyCode> equiCurrency;
    } opt;
  } in;

  struct Out {
    struct CurrencyPart {
      auto operator<=>(const CurrencyPart &) const = default;

      MonetaryAmount a;
      std::optional<MonetaryAmount> eq;
    };

    using ExchangeKeyPart = vector<std::pair<CurrencyCode, CurrencyPart>>;
    using ExchangePart = SmallVector<std::pair<std::string_view, ExchangeKeyPart>, 1>;

    FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> exchange;

    struct TotalPart {
      ExchangeKeyPart cur;
      std::optional<MonetaryAmount> eq;
    } total;
  } out;
};

struct DepositInfo {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::DepositInfo;
    struct Opt {
      CurrencyCode cur;
    } opt;
  } in;

  struct ExchangeKeyPart {
    std::string_view address;
    std::optional<std::string_view> tag;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, ExchangeKeyPart>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct Trades {
  struct In {
    CoincenterCommandType req{};
    struct Opt {
      struct FromTo {
        std::optional<MonetaryAmount> amount;
        CurrencyCode currency;
        std::optional<bool> isPercentage;
      };

      std::optional<FromTo> from;
      std::optional<FromTo> to;

      struct Options {
        struct Price {
          PriceStrategy strategy;
          std::optional<MonetaryAmount> fixedPrice;
          std::optional<int> relativePrice;
        } price;
        Duration maxTradeTime;
        Duration minTimeBetweenPriceUpdates;
        TradeMode mode;
        TradeSyncPolicy syncPolicy;
        TradeTimeoutAction timeoutAction;
      } options;

    } opt;
  } in;

  struct ExchangeKeyPart {
    MonetaryAmount from;
    TradeResult::State status;
    MonetaryAmount tradedFrom;
    MonetaryAmount tradedTo;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, ExchangeKeyPart>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct Orders {
  struct In {
    CoincenterCommandType req{};
    struct Opt {
      std::optional<CurrencyCode> cur1;
      std::optional<CurrencyCode> cur2;
      std::optional<TimePointIso8601UTC> placedBefore;
      std::optional<TimePointIso8601UTC> placedAfter;
      std::optional<std::span<const OrderId>> matchIds;
    };

    std::optional<Opt> opt;
  } in;

  struct Order {
    auto operator<=>(const Order &) const = default;

    std::string_view id;
    Market pair;
    TimePointIso8601UTC placedTime;
    std::optional<TimePointIso8601UTC> matchedTime;
    TradeSide side;
    MonetaryAmount price;
    MonetaryAmount matched;
    std::optional<MonetaryAmount> remaining;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, vector<Order>>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct OrdersCancelled {
  Orders::In in;

  struct Elem {
    int nb;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, Elem>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct RecentDeposits {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::RecentDeposits;
    struct Opt {
      std::optional<CurrencyCode> cur;
      std::optional<TimePointIso8601UTC> receivedBefore;
      std::optional<TimePointIso8601UTC> sentBefore;
      std::optional<TimePointIso8601UTC> receivedAfter;
      std::optional<TimePointIso8601UTC> sentAfter;
      std::optional<std::span<const OrderId>> matchIds;
    };

    std::optional<Opt> opt;
  } in;

  struct Elem {
    std::string_view id;
    CurrencyCode cur;
    TimePointIso8601UTC receivedTime;
    MonetaryAmount amount;
    WithdrawOrDeposit::Status status;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, vector<Elem>>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct RecentWithdraws {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::RecentWithdraws;

    std::optional<RecentDeposits::In::Opt> opt;
  } in;

  struct Elem {
    std::string_view id;
    CurrencyCode cur;
    TimePointIso8601UTC sentTime;
    MonetaryAmount netEmittedAmount;
    MonetaryAmount fee;
    WithdrawOrDeposit::Status status;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, vector<Elem>>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct Conversion1 {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Conversion;
    struct Opt {
      MonetaryAmount fromAmount;
      CurrencyCode fromCurrency;
      CurrencyCode toCurrency;
    };

    Opt opt;
  } in;

  struct ExchangePart {
    MonetaryAmount convertedAmount;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct Conversion2 {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Conversion;
    struct Opt {
      struct ExchangePart {
        auto operator<=>(const ExchangePart &) const = default;

        MonetaryAmount amount;
        CurrencyCode cur;
      };
      FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> fromAmount;
      CurrencyCode toCurrency;
    };

    Opt opt;
  } in;

  struct ExchangePart {
    MonetaryAmount convertedAmount;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct ConversionPath {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::ConversionPath;
    struct Opt {
      Market market;
    };

    Opt opt;
  } in;

  FixedCapacityVector<std::pair<ExchangeNameEnum, std::span<const Market>>, kNbSupportedExchanges> out;
};

struct WithdrawFees {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::WithdrawFees;
    struct Opt {
      std::optional<CurrencyCode> cur;
    };

    Opt opt;
  } in;

  FixedCapacityVector<std::pair<ExchangeNameEnum, std::span<const MonetaryAmount>>, kNbSupportedExchanges> out;
};

struct Last24hTradedVolume {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Last24hTradedVolume;
    struct Opt {
      Market market;
    };

    Opt opt;
  } in;

  FixedCapacityVector<std::pair<ExchangeNameEnum, MonetaryAmount>, kNbSupportedExchanges> out;
};

struct LastTrades {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::LastTrades;
    struct Opt {
      Market market;
      std::optional<int> nb;
    };

    Opt opt;
  } in;

  struct Elem {
    MonetaryAmount a;
    MonetaryAmount p;
    TimePointIso8601UTC time;
    TradeSide side;
  };

  FixedCapacityVector<std::pair<ExchangeNameEnum, vector<Elem>>, kNbSupportedExchanges> out;
};

struct LastPrice {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::LastPrice;
    struct Opt {
      Market market;
    };

    Opt opt;
  } in;

  FixedCapacityVector<std::pair<ExchangeNameEnum, MonetaryAmount>, kNbSupportedExchanges> out;
};

struct Withdraw {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::Withdraw;
    struct Opt {
      CurrencyCode cur;
      bool isPercentage;
      WithdrawSyncPolicy syncPolicy;
    };

    Opt opt;
  } in;

  struct Out {
    struct From {
      ExchangeNameEnum exchange;
      std::string_view account;
      std::string_view id;
      MonetaryAmount amount;
      TimePointIso8601UTC time;
    } from;
    struct To {
      ExchangeNameEnum exchange;
      std::string_view account;
      std::string_view id;
      MonetaryAmount amount;
      std::string_view address;
      std::optional<std::string_view> tag;
      TimePointIso8601UTC time;
    } to;
  } out;
};

struct DustSweeper {
  struct In {
    CoincenterCommandType req = CoincenterCommandType::DustSweeper;
    struct Opt {
      CurrencyCode cur;
    };

    Opt opt;
  } in;

  struct ExchangeKeyPart {
    struct TradedAmounts {
      auto operator<=>(const TradedAmounts &) const = default;

      MonetaryAmount from;
      MonetaryAmount to;
    };
    vector<TradedAmounts> trades;
    MonetaryAmount finalAmount;
  };

  using ExchangePart = SmallVector<std::pair<std::string_view, ExchangeKeyPart>, 1>;

  FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> out;
};

struct MarketTradingResults {
  struct In {
    CoincenterCommandType req{};
    struct Opt {
      struct Time {
        TimePointIso8601UTC from;
        TimePointIso8601UTC to;
      } time;
    };

    Opt opt;
  } in;

  struct MarketTradingResult {
    std::string_view algorithm;
    Market market;
    struct StartAmounts {
      MonetaryAmount base;
      MonetaryAmount quote;
    } startAmounts;
    MonetaryAmount profitAndLoss;
    struct Stats {
      struct TradeRangeResultsStats {
        int nbError;
        int nbSuccessful;
        struct Time {
          TimePointIso8601UTC from;
          TimePointIso8601UTC to;
        } time;
      };
      TradeRangeResultsStats orderBooks;
      TradeRangeResultsStats trades;
    } stats;

    vector<Orders::Order> matchedOrders;
  };

  using ExchangeMarketResults =
      FixedCapacityVector<std::pair<ExchangeNameEnum, MarketTradingResult>, kNbSupportedExchanges>;

  using AllResults = vector<ExchangeMarketResults>;

  using AlgorithmNameResults = vector<AllResults>;

  vector<std::pair<std::string_view, AlgorithmNameResults>> out;
};

}  // namespace cct::schema::queryresult