#pragma once

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "balanceportfolio.hpp"
#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "exchangepublicapi.hpp"
#include "exchangeretriever.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "wallet.hpp"

namespace cct {
class Exchange;

using MarketOrderBookConversionRate = std::tuple<std::string_view, MarketOrderBook, std::optional<MonetaryAmount>>;
using MarketOrderBookConversionRates = FixedCapacityVector<MarketOrderBookConversionRate, kNbSupportedExchanges>;
using MarketsPerExchange =
    FixedCapacityVector<std::pair<const Exchange *, api::ExchangePublic::MarketSet>, kNbSupportedExchanges>;
using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;
using MonetaryAmountPerExchange =
    FixedCapacityVector<std::pair<const Exchange *, MonetaryAmount>, kNbSupportedExchanges>;
using LastTradesPerExchange =
    FixedCapacityVector<std::pair<const Exchange *, api::ExchangePublic::LastTradesVector>, kNbSupportedExchanges>;
using MarketOrderBookMaps = FixedCapacityVector<api::ExchangePublic::MarketOrderBookMap, kNbSupportedExchanges>;
using ExchangeTickerMaps = std::pair<ExchangeRetriever::PublicExchangesVec, MarketOrderBookMaps>;
using BalancePerExchange = SmallVector<std::pair<const Exchange *, BalancePortfolio>, kTypicalNbPrivateAccounts>;
using WalletPerExchange = SmallVector<std::pair<const Exchange *, Wallet>, kTypicalNbPrivateAccounts>;
using ConversionPathPerExchange =
    FixedCapacityVector<std::pair<const Exchange *, api::ExchangePublic::ConversionPath>, kNbSupportedExchanges>;
using WithdrawFeePerExchange = FixedCapacityVector<std::pair<const Exchange *, MonetaryAmount>, kNbSupportedExchanges>;
}  // namespace cct