#pragma once

#include <optional>
#include <utility>

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "exchangepublicapitypes.hpp"
#include "monetaryamount.hpp"
#include "time-window.hpp"
#include "timepoint-schema.hpp"

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
    TimePoint time;
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
    using ExchangePart = SmallVector<std::pair<string, ExchangeKeyPart>, 1>;

    FixedCapacityVector<std::pair<ExchangeNameEnum, ExchangePart>, kNbSupportedExchanges> exchange;

    struct TotalPart {
      ExchangeKeyPart cur;
      std::optional<MonetaryAmount> eq;
    } total;
  } out;
};

}  // namespace cct::schema::queryresult