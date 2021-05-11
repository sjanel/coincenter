#include <forward_list>
#include <iostream>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_config.hpp"
#include "cct_log.hpp"
#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "coincenteroptions.hpp"
#include "commandlineoptionsparser.hpp"
#include "cryptowatchapi.hpp"
#include "curlhandle.hpp"
#include "exchange.hpp"
#include "fiatconverter.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"
#include "stringoptionparser.hpp"
#include "tradeoptionsapi.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {
namespace {

int main(int argc, const char* argv[]) {
  MonetaryAmount startTradeAmount;
  CurrencyCode toTradeCurrency;
  PrivateExchangeName tradePrivateExchangeName;
  api::TradeOptions tradeOptions;

  Market marketForOrderBook;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth = 0;
  CurrencyCode orderbookCur(CurrencyCode::kNeutral);

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  MonetaryAmount amountToWithdraw("0");
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;

  try {
    CommandLineOptionsParser<CoincenterCmdLineOptions> coincenterCommandLineOptionsParser =
        CreateCoincenterCommandLineOptionsParser<CoincenterCmdLineOptions>();
    CoincenterCmdLineOptions cmdLineOptions = coincenterCommandLineOptionsParser.parse(argc, argv);
    if (cmdLineOptions.help) {
      coincenterCommandLineOptionsParser.displayHelp(argv[0], std::cout);
      return EXIT_SUCCESS;
    }
    if (cmdLineOptions.version) {
      CoincenterCmdLineOptions::PrintVersion(argv[0]);
      return EXIT_SUCCESS;
    }
    log::set_level(log::level::info);

    cmdLineOptions.setLogLevel();
    cmdLineOptions.setLogFile();

    if (!cmdLineOptions.orderbook.empty()) {
      AnyParser anyParser(cmdLineOptions.orderbook);
      std::tie(marketForOrderBook, orderBookExchanges) = anyParser.getMarketExchanges();

      orderbookDepth = cmdLineOptions.orderbook_depth;
      orderbookCur = CurrencyCode(cmdLineOptions.orderbook_cur);
    }

    if (!cmdLineOptions.conversion_path.empty()) {
      AnyParser anyParser(cmdLineOptions.conversion_path);
      std::tie(marketForConversionPath, conversionPathExchanges) = anyParser.getMarketExchanges();
    }

    if (!cmdLineOptions.balance.empty()) {
      AnyParser anyParser(cmdLineOptions.balance);
      balancePrivateExchanges = anyParser.getPrivateExchanges();
      balanceCurrencyCode = CurrencyCode(cmdLineOptions.balance_cur);
    }

    if (!cmdLineOptions.trade.empty()) {
      AnyParser anyParser(cmdLineOptions.trade);
      std::tie(startTradeAmount, toTradeCurrency, tradePrivateExchangeName) =
          anyParser.getMonetaryAmountCurrencyCodePrivateExchange();

      tradeOptions = api::TradeOptions(
          cmdLineOptions.trade_strategy,
          cmdLineOptions.trade_sim ? api::TradeOptions::Mode::kSimulation : api::TradeOptions::Mode::kReal,
          std::chrono::seconds(cmdLineOptions.trade_timeout_s),
          std::chrono::milliseconds(cmdLineOptions.trade_emergency_ms),
          std::chrono::milliseconds(cmdLineOptions.trade_updateprice_ms));
    }

    if (!cmdLineOptions.withdraw.empty()) {
      AnyParser anyParser(cmdLineOptions.withdraw);
      std::tie(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
          anyParser.getMonetaryAmountFromToPrivateExchange();
    }

  } catch (const InvalidArgumentException& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  CurlInitRAII curlInitRAII;
  CoincenterInfo coincenterInfo;
  api::CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  api::APIKeysProvider apiKeyProvider;

  // Public exchanges
  api::KrakenPublic krakenPublicAPI(coincenterInfo, fiatConverter, cryptowatchAPI);
  api::BithumbPublic bithumbPublicAPI(coincenterInfo, fiatConverter, cryptowatchAPI);
  api::BinancePublic binancePublic(coincenterInfo, fiatConverter, cryptowatchAPI);
  api::UpbitPublic upbitPublic(coincenterInfo, fiatConverter, cryptowatchAPI);

  // Private exchanges (based on provided keys)
  // Use forward_list to guarantee validity of the iterators and pointers, as we give them to Exchange object as
  // pointers
  ExchangeVector exchanges;
  std::forward_list<api::KrakenPrivate> krakenPrivates;
  std::forward_list<api::BinancePrivate> binancePrivates;
  std::forward_list<api::BithumbPrivate> bithumbPrivates;
  std::forward_list<api::UpbitPrivate> upbitPrivates;
  for (std::string_view exchangeName : api::ExchangePublic::kSupportedExchanges) {
    api::ExchangePublic* exchangePublic;
    if (exchangeName == "kraken") {
      exchangePublic = std::addressof(krakenPublicAPI);
    } else if (exchangeName == "binance") {
      exchangePublic = std::addressof(binancePublic);
    } else if (exchangeName == "bithumb") {
      exchangePublic = std::addressof(bithumbPublicAPI);
    } else if (exchangeName == "upbit") {
      exchangePublic = std::addressof(upbitPublic);
    } else {
      throw exception("Should not happen, unsupported platform " + std::string(exchangeName));
    }

    const bool canUsePrivateExchange = apiKeyProvider.contains(exchangeName);
    if (canUsePrivateExchange) {
      for (const std::string& keyName : apiKeyProvider.getKeyNames(exchangeName)) {
        api::ExchangePrivate* exchangePrivate;
        const api::APIKey& apiKey = apiKeyProvider.get(PrivateExchangeName(exchangeName, keyName));
        if (exchangeName == "kraken") {
          exchangePrivate = std::addressof(krakenPrivates.emplace_front(coincenterInfo, krakenPublicAPI, apiKey));
        } else if (exchangeName == "binance") {
          exchangePrivate = std::addressof(binancePrivates.emplace_front(coincenterInfo, binancePublic, apiKey));
        } else if (exchangeName == "bithumb") {
          exchangePrivate = std::addressof(bithumbPrivates.emplace_front(coincenterInfo, bithumbPublicAPI, apiKey));
        } else if (exchangeName == "upbit") {
          exchangePrivate = std::addressof(upbitPrivates.emplace_front(coincenterInfo, upbitPublic, apiKey));
        } else {
          throw exception("Should not happen, unsupported platform " + std::string(exchangeName));
        }

        exchanges.emplace_back(coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic, *exchangePrivate);
      }
    } else {
      exchanges.emplace_back(coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic);
    }
  }

  Coincenter coincenter(coincenterInfo, fiatConverter, cryptowatchAPI, std::move(exchanges));

  if (!orderBookExchanges.empty()) {
    std::optional<int> depth;
    if (orderbookDepth != 0) {
      depth = orderbookDepth;
    }
    Coincenter::MarketOrderBookConversionRates marketOrderBooksConversionRates =
        coincenter.getMarketOrderBooks(marketForOrderBook, orderBookExchanges, orderbookCur, depth);
    int orderBookPos = 0;
    for (std::string_view exchangeName : orderBookExchanges) {
      const auto& [marketOrderBook, optConversionRate] = marketOrderBooksConversionRates[orderBookPos];
      log::info("Order book of {} on {} requested{}{}", marketForOrderBook.str(), exchangeName,
                optConversionRate ? " with conversion rate " : "", optConversionRate ? optConversionRate->str() : "");

      if (optConversionRate) {
        marketOrderBook.print(std::cout, *optConversionRate);
      } else {
        if (orderbookCur != CurrencyCode::kNeutral) {
          log::warn("Unable to convert {} into {} on {}", marketForOrderBook.quote().str(), orderbookCur.str(),
                    exchangeName);
        }
        marketOrderBook.print(std::cout);
      }

      ++orderBookPos;
    }
  }

  if (!conversionPathExchanges.empty()) {
    coincenter.printConversionPath(conversionPathExchanges, marketForConversionPath.base(),
                                   marketForConversionPath.quote());
  }

  if (!balancePrivateExchanges.empty()) {
    coincenter.printBalance(balancePrivateExchanges, balanceCurrencyCode);
  }

  if (!startTradeAmount.isZero()) {
    log::warn("Trade {} into {} on {} requested", startTradeAmount.str(), toTradeCurrency.str(),
              tradePrivateExchangeName.str());
    log::warn(tradeOptions.str());
    MonetaryAmount startAmount = startTradeAmount;
    MonetaryAmount toAmount = coincenter.trade(startAmount, toTradeCurrency, tradePrivateExchangeName, tradeOptions);
    log::warn("**** Traded {} into {} ****", (startTradeAmount - startAmount).str(), toAmount.str());
  }

  if (!amountToWithdraw.isZero()) {
    log::warn("Withdraw gross {} from {} to {} requested", amountToWithdraw.str(), withdrawFromExchangeName.str(),
              withdrawToExchangeName.str());
    coincenter.withdraw(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName);
  }

  coincenter.updateFileCaches();

  return EXIT_SUCCESS;
}
}  // namespace
}  // namespace cct

int main(int argc, const char* argv[]) { return cct::main(argc, argv); }
