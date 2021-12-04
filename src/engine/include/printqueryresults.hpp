#pragma once

#include "currencycode.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {
void PrintMarkets(CurrencyCode cur, const MarketsPerExchange &marketsPerExchange);

void PrintMarketOrderBooks(const MarketOrderBookConversionRates &marketOrderBooksConversionRates);

void PrintTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps);

void PrintBalance(const BalancePerExchange &balancePerExchange);

void PrintDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange);

void PrintConversionPath(Market m, const ConversionPathPerExchange &conversionPathsPerExchange);

void PrintWithdrawFees(const WithdrawFeePerExchange &withdrawFeePerExchange);

void PrintLast24hTradedVolume(Market m, const MonetaryAmountPerExchange &tradedVolumePerExchange);

void PrintLastTrades(Market m, const LastTradesPerExchange &lastTradesPerExchange);

void PrintLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange);
}  // namespace cct