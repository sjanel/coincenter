# Configuration

At this step, `coincenter` is built. To execute properly, it needs read/write access to a special directory `data` which contains a tree of files as follows:

- `cache`: Files containing cache data aiming to reduce external calls to some costly services. They are typically read at the start of the program, and flushed at the normal termination of the program, potentially with updated data retrieved dynamically during the run. It is not thread-safe: only one `coincenter` service should have access to it at the same time.
- `log`: Files that store history of logs and activity of relevant commands. It can be configured with the log levels of your choice thanks to `generalconfig.json` file. It can contain sensitive information (such as history of buy, sells, withdraws) and will not be included in any docker build or git
- `secret`: contains all **extremely** sensitive information and data such as secrets and deposit addresses. Do not share or publish this folder! If you did it by mistake, immediately delete the keys from each of the compromised accounts and generate new ones for each exchange.
- `static`: contains data which is not supposed to be updated regularly, typically loaded once at start up of `coincenter` and not updated automatically. `exchangeconfig.json` contains various options which can control general behavior of `coincenter`. If none is found, a default one will be generated automatically, which you can later on update according to your needs. `generalconfig.json` contains general options independent from exchanges (such as logging, fiat converter).

This directory is set according to these rules, by decreasing priority:

- `--data` option from the command line
- or from `CCT_DATA_DIR` environment variable if it is set at runtime
- or defaults to the default data directory chosen at build time from `CCT_DATA_DIR` environment variable

## Important files

### secret/secret.json

Fill this file with your private keys for each of your account(s) in the exchanges. 
Of course, no need to say that this file should be kept secret, and not transit in the internet, or any other *Docker* image/layer or *git* commit. 
It is present in `.gitignore` and `.dockerignore` to avoid accidents.

For additional security, always bind your keys to your IP (some exchanges will force you to do it anyway), especially if you wish to use it for sensitive commands such as withdraws.

[data/secret/secret_test.json](data/secret/secret_test.json) shows the syntax.

#### Additional information

##### Kucoin

In addition of the `key` and `private` values, you will need to provide your `passphrase` as well. It's provided as a string field along with your key in the `secret.json` file.

##### Bithumb

Bithumb **withdrawals** have an extra personal information check: they need to know the **English** and **Korean** name of the owner of the destination account.
Thus it's also an information that you need to provide in the keys of your targeted accounts for a Bithumb withdrawal.
For instance, if you wish to withdraw a coin from your **Bithumb** to your **Upbit** account from `coincenter`, you need to specify your name in the **Upbit** key used as destination of the withdraw.
Korean name should be provided with the `accountOwner.koName` field in the key of the `secret.json` file, whereas English name with the `accountOwner.enName`.

Refer to the [data/secret/secret_test.json](data/secret/secret_test.json) example file in case of doubt.

### static/generalconfig.json

Contains options that are not exchange specific.

Configures the logging, tracking activity of relevant commands, and console output type.

#### General options description

| Name                                           | Value                                    | Description                                                                                                                                                                                                                                                                                                     |
| ---------------------------------------------- | ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **apiOutputType**                              | String among {`off`, `table`, `json`}    | Configure the default output type of coincenter (can be overridden by command line)queries                                                                                                                                                                                                                      |
| **fiatConversion.rate**                        | Duration string (ex: `8h`)               | Minimum duration between two consecutive requests of the same fiat conversion                                                                                                                                                                                                                                   |
| **log.activityTracking.commandTypes**          | Array of strings (ex: `["Buy", "Sell"]`) | Array of command types whose output will be stored to activity history files.                                                                                                                                                                                                                                   |
| **log.activityTracking.dateFileNameFormat**    | String (ex: `%Y-%m` for month split)     | Defines the date string format suffix used by activity history files. The string should be compatible with [std::strftime](https://en.cppreference.com/w/cpp/chrono/c/strftime). Old data will never be clean-up by `coincenter` (as it may contain important data). User should manage the clean-up / storage. |
| **log.activityTracking.withSimulatedCommands** | Boolean                                  | When some commands are launched in simulated mode (trades, withdraw for instance), they will be logged if `true`.                                                                                                                                                                                               |
| **log.consoleLevel**                           | String                                   | Defines the log level for standard output. Can be {'off', 'critical', 'error', 'warning', 'info', 'debug', 'trace'}                                                                                                                                                                                             |
| **log.fileLevel**                              | String                                   | Defines the log level in files. Can be {'off', 'critical', 'error', 'warning', 'info', 'debug', 'trace'}                                                                                                                                                                                                        |
| **log.maxFileSize**                            | String (ex: `5Mi` for 5 Megabytes)       | Defines in bytes the maximum logging file size. A string representation of an integral, possibly with one suffix ending such as k, M, G, T (1k multipliers) or Ki, Mi, Gi, Ti (1024 multipliers) are supported.                                                                                                 |
| **log.maxNbFiles**                             | Integer                                  | Number of maximum rotating files for log in files                                                                                                                                                                                                                                                               |
| **requests.concurrency.nbMaxParallelRequests** | Integer                                  | Size of the thread pool that makes exchange requests.                                                                                                                                                                                                                                                           |

### static/exchangeconfig.json

This json file should follow this specific format:

```yaml
  - top_level_option:
    - default:
      - some_option: default_value
      - another_option: default_value
    - exchange:
      - some_exchange:
        - some_option: override_value
        - another_option: default_value
      - another_exchange:
        - some_option: override_value
```

Currently, options are set from two ways:

- **Comma separated values** are aggregated for each exchange with the 'default' values (if present)
- **Single values** are retrieved in a 'bottom first' priority model, meaning that if a value is specified for an exchange name, it is chosen. Otherwise, it checks at the default value for this option, and if again not present, uses a hardcoded default one (cf in the code).

As an example, consider this file:

```json
{
  "asset": {
    "default": {
      "withdrawExclude": "BTC"
    },
    "exchange": {
      "binance": {
        "withdrawExclude": "BQX"
      },
      "kraken": {
        "withdrawExclude": "EUR,KFEE"
      }
    }
  },
  "tradefees": {
    "default": {
      "maker": "0.1",
    },
    "exchange": {
      "bithumb": {
        "maker": "0.25",
      }
    }
  }
}
```

The chosen values will be:

| Exchange | `asset/withdrawExclude` | `tradefees/maker` |
| -------- | ----------------------- | ----------------- |
| Binance  | `BTC,BQX`               | `0.1`             |
| Kraken   | `BTC,EUR,KFEE`          | `0.1`             |
| Bithumb  | `BTC`                   | `0.25`            |

Refer to the hardcoded default json example as a model in case of doubt.

#### Exchanges options description

| Module      | Name                               | Value                                                                            | Description                                                                                                                                                                                                                                                                                                                                                                                                              |
| ----------- | ---------------------------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| *asset*     | **allExclude**                     | Array of coin acronyms (ex: `["EUR", "AUD"]`))                                   | Exclude coins with these acronym from `coincenter`                                                                                                                                                                                                                                                                                                                                                                       |
| *asset*     | **withdrawExclude**                | Array of coin acronyms (ex: `["BTC", "ETC"]`))                                   | Make these coins unavailable for withdraw                                                                                                                                                                                                                                                                                                                                                                                |
| *asset*     | **preferredPaymentCurrencies**     | Ordered array of coin acronyms (ex: `["USDT", "BTC"]`))                          | Coins that can be used for smart buy and sell as base payment currency. They should be ordered by decreasing priority.                                                                                                                                                                                                                                                                                                   |
| *query*     | **acceptEncoding**                 | Comma separated list of accepted encodings (ex: `"br, gzip, deflate"`), or empty | Sets list of accepted encodings that will be passed to `curl` requests as `Accept-Encoding` header. More information [here](https://curl.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html)                                                                                                                                                                                                                                      |
| *query*     | **dustAmountsThreshold**           | Unordered array of monetary amounts (ex: `["1 USDT", "0.000001 BTC"]`)           | For dust sweeper option, if total balance of a currency is convertible to any of these monetary amounts and if their amount is under the threshold, it will be eligible for the selling                                                                                                                                                                                                                                  |
| *query*     | **logLevels.requestsCall**         | String log level for requests call ("off", "critical", "warning", "info", etc)   | Specifies the log level for this exchange requests call. It prints the full public URL and the HTTP request type (GET, POST, etc)                                                                                                                                                                                                                                                                                        |
| *query*     | **logLevels.requestsAnswer**       | String log level for requests call ("off", "critical", "warning", "info", etc)   | Specifies the log level for this exchange requests replies. It prints the full answer if it is in *json* type, otherwise it will be truncated to a maximum of around 10 Ki to avoid logging too much data.                                                                                                                                                                                                               |
| *query*     | **dustSweeperMaxNbTrades**         | Positive integer                                                                 | Maximum number of trades performed by the automatic dust sweeper process. A high value may have a higher chance of successfully sell to 0 the wanted currency, at the cost of more trades (and fees) paid to the exchange.                                                                                                                                                                                               |
| *query*     | **http.timeout**                   | Duration string (ex: `15s`)                                                      | Sets the timeout duration for the HTTP requests of the exchanges.                                                                                                                                                                                                                                                                                                                                                        |
| *query*     | **privateAPIRate**                 | Duration string (ex: `500ms`)                                                    | Minimum duration between two consecutive requests of private account                                                                                                                                                                                                                                                                                                                                                     |
| *query*     | **publicAPIRate**                  | Duration string (ex: `250ms`)                                                    | Minimum duration between two consecutive requests of public account                                                                                                                                                                                                                                                                                                                                                      |
| *query*     | **trade.minPriceUpdateDuration**   | Duration string (ex: `30s`)                                                      | Minimum duration between two consecutive price changes during trade                                                                                                                                                                                                                                                                                                                                                      |
| *query*     | **trade.strategy**                 | <`maker`, `nibble`, `taker`>                                                     | Trade strategy for the exchange. It will be the default for the exchange if not manually specified.                                                                                                                                                                                                                                                                                                                      |
| *query*     | **trade.timeout**                  | Duration string (ex: `1m`)                                                       | Trade timeout duration for a single trade. It will be the default for the exchange if not manually specified.                                                                                                                                                                                                                                                                                                            |
| *query*     | **trade.timeoutMatch**             | Boolean (`true` or `false`)                                                      | If `false`, the remaining unmatched order is cancelled for the trade when it reaches timeout duration specified above. If `true`, trade will be placed as a taker order when the timeout is reached.                                                                                                                                                                                                                     |
| *query*     | **updateFrequency.currencies**     | Duration string (ex: `4h`)                                                       | Minimum duration between two consecutive requests of currencies                                                                                                                                                                                                                                                                                                                                                          |
| *query*     | **updateFrequency.markets**        | Duration string (ex: `4h`)                                                       | Minimum duration between two consecutive requests of markets                                                                                                                                                                                                                                                                                                                                                             |
| *query*     | **updateFrequency.withdrawalFees** | Duration string (ex: `12h`)                                                      | Minimum duration between two consecutive requests of withdrawal fees                                                                                                                                                                                                                                                                                                                                                     |
| *query*     | **updateFrequency.allOrderbooks**  | Duration string (ex: `2s`)                                                       | Minimum duration between two consecutive requests of all order books (or ticker)                                                                                                                                                                                                                                                                                                                                         |
| *query*     | **updateFrequency.orderbook**      | Duration string (ex: `1s`)                                                       | Minimum duration between two consecutive requests of a single orderbook                                                                                                                                                                                                                                                                                                                                                  |
| *query*     | **updateFrequency.tradedVolume**   | Duration string (ex: `4h`)                                                       | Minimum duration between two consecutive requests of last traded volume                                                                                                                                                                                                                                                                                                                                                  |
| *query*     | **updateFrequency.lastPrice**      | Duration string (ex: `1s500ms`)                                                  | Minimum duration between two consecutive requests of price                                                                                                                                                                                                                                                                                                                                                               |
| *query*     | **updateFrequency.depositWallet**  | Duration string (ex: `1min`)                                                     | Minimum duration between two consecutive requests of deposit information (including wallet)                                                                                                                                                                                                                                                                                                                              |
| *query*     | **updateFrequency.currencyInfo**   | Duration string (ex: `4h`)                                                       | Minimum duration between two consecutive requests of dynamic currency info retrieval on Bithumb only (used for place order)                                                                                                                                                                                                                                                                                              |
| *query*     | **placeSimulateRealOrder**         | Boolean (`true` or `false`)                                                      | If `true`, in trade simulation mode (with `--sim`) exchanges which do not support simulated mode in place order will actually place a real order, with the following characteristics: <ul><li>trade strategy forced to `maker`</li><li>price will be changed to a maximum for a sell, to a minimum for a buy</li></ul> This will allow place of a 'real' order that cannot be matched in practice (if it is, lucky you!) |
| *query*     | **multiTradeAllowedByDefault**     | Boolean (`true` or `false`)                                                      | If `true`, [multi-trade](README.md#multi-trade) will be allowed by default for `trade`, `buy` and `sell`. It can be overridden at command line level with `--no-multi-trade` and `--multi-trade`.                                                                                                                                                                                                                        |
| *query*     | **validateApiKey**                 | Boolean (`true` or `false`)                                                      | If `true`, each loaded private key will be tested at start of the program. In case of a failure, it will be removed from the list of private accounts loaded by `coincenter`, so that later queries do not consider it instead of raising a runtime exception. The downside is that it will make an additional check that will make startup slower.                                                                      |  |
| *tradefees* | **maker**                          | String as decimal number representing a percentage (for instance, "0.15")        | Trade fees occurring when a maker order is matched                                                                                                                                                                                                                                                                                                                                                                       |
| *tradefees* | **taker**                          | String as decimal number representing a percentage (for instance, "0.15")        | Trade fees occurring when a taker order is matched                                                                                                                                                                                                                                                                                                                                                                       |
| *withdraw*  | **validateDepositAddressesInFile** | Boolean (`true` or `false`)                                                      | If `true`, each withdraw will perform an additional validation check from a trusted list of "whitelisted" addresses in `depositaddresses.json` file. Withdraw will not be processed if destination wallet is not present in the file.                                                                                                                                                                                    |

##### Notes

- `updateFrequency` is itself a json document containing all duration values as query frequencies. 
  See [ExchangeConfig default file](src/objects/src/exchangeconfigdefault.hpp) as an example for the syntax.
- Unused and not explicitly set values (so, when loaded from default values) from your personal `exchangeconfig.json` file will be logged for information about what will actually be used by `coincenter`.
