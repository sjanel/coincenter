# Configuration

At this step, `coincenter` is built. To execute properly, it needs read/write access to a special directory `data` which contains a tree of files as follows:

- `cache`: Files containing cache data aiming to reduce external calls to some costly services. They are typically read at the start of the program, and flushed at the normal termination of the program, potentially with updated data retrieved dynamically during the run. It is not thread-safe: only one `coincenter` service should have access to it at the same time.
- `secret`: contains all sensitive information and data such as secrets and deposit addresses. Do not share or publish this folder!
- `static`: contains data which is not supposed to be updated regularly, typically loaded once at start up of `coincenter` and not updated automatically. `exchangeconfig.json` contains various options which can control general behavior of `coincenter`. If none is found, a default one will be generated automatically, which you can later on update according to your needs.

## Important files

### secret/secret.json

Fill this file with your private keys for each of your account(s) in the exchanges. 
Of course, no need to say that this file should be kept secret, and not transit in the internet, or any other *Docker* image or *git* commit. 
It is present in `.gitignore` and `.dockerignore` to avoid accidents. 
For additional security, always bind your keys to your IP (some exchanges will force you to do it anyway).

`<DataDir>/secret/secret_test.json` shows the syntax.

For *Kucoin*, in addition of the `key` and `private` values, you will need to provide your `passphrase` as well.

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

#### Option descriptions

| Module      | Name                               | Value                                                                     | Description                                                                                                                                                                                                                                                                                                                                                                                                                    |
| ----------- | ---------------------------------- | ------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| *asset*     | **allExclude**                     | CSV coin acronyms (ex: "BTC,AUD")                                         | Exclude coins with these acronym from `coincenter`                                                                                                                                                                                                                                                                                                                                                                             |
| *asset*     | **withdrawExclude**                | CSV coin acronyms (ex: "BTC,AUD")                                         | Make these coins unavailable for withdraw                                                                                                                                                                                                                                                                                                                                                                                      |
| *query*     | **privateAPIRate**                 | Duration string (ex: `500ms`)                                             | Minimum time between two consecutive requests of private account                                                                                                                                                                                                                                                                                                                                                               |
| *query*     | **publicAPIRate**                  | Duration string (ex: `250ms`)                                             | Minimum time between two consecutive requests of public account                                                                                                                                                                                                                                                                                                                                                                |
| *query*     | **updateFrequency.currencies**     | Duration string (ex: `4h`)                                                | Minimum time between two consecutive requests of currencies                                                                                                                                                                                                                                                                                                                                                                    |
| *query*     | **updateFrequency.markets**        | Duration string (ex: `4h`)                                                | Minimum time between two consecutive requests of markets                                                                                                                                                                                                                                                                                                                                                                       |
| *query*     | **updateFrequency.withdrawalFees** | Duration string (ex: `12h`)                                               | Minimum time between two consecutive requests of withdrawal fees                                                                                                                                                                                                                                                                                                                                                               |
| *query*     | **updateFrequency.allOrderbooks**  | Duration string (ex: `2s`)                                                | Minimum time between two consecutive requests of all order books (or ticker)                                                                                                                                                                                                                                                                                                                                                   |
| *query*     | **updateFrequency.orderbook**      | Duration string (ex: `1s`)                                                | Minimum time between two consecutive requests of a single orderbook                                                                                                                                                                                                                                                                                                                                                            |
| *query*     | **updateFrequency.tradedVolume**   | Duration string (ex: `4h`)                                                | Minimum time between two consecutive requests of last traded volume                                                                                                                                                                                                                                                                                                                                                            |
| *query*     | **updateFrequency.lastPrice**      | Duration string (ex: `1s500ms`)                                           | Minimum time between two consecutive requests of price                                                                                                                                                                                                                                                                                                                                                                         |
| *query*     | **updateFrequency.depositWallet**  | Duration string (ex: `1min`)                                              | Minimum time between two consecutive requests of deposit information (including wallet)                                                                                                                                                                                                                                                                                                                                        |
| *query*     | **updateFrequency.nbDecimals**     | Duration string (ex: `4h`)                                                | Minimum time between two consecutive requests of get number of decimals on Bithumb only (used for place order)                                                                                                                                                                                                                                                                                                                 |
| *query*     | **placeSimulateRealOrder**         | Boolean (`true` or `false`)                                               | If `true`, in trade simulation mode (with `--trade-sim`) exchanges which do not support simulated mode in place order will actually place a real order, with the following characteristics: <ul><li>trade strategy forced to `maker`</li><li>price will be changed to a maximum for a sell, to a minimum for a buy</li></ul> This will allow place of a 'real' order that cannot be matched in practice (if it is, lucky you!) |
| *tradefees* | **maker**                          | String as decimal number representing a percentage (for instance, "0.15") | Trade fees occurring when a maker order is matched                                                                                                                                                                                                                                                                                                                                                                             |
| *tradefees* | **taker**                          | String as decimal number representing a percentage (for instance, "0.15") | Trade fees occurring when a taker order is matched                                                                                                                                                                                                                                                                                                                                                                             |
| *withdraw*  | **validateDepositAddressesInFile** | Boolean (`true` or `false`)                                               | If `true`, each withdraw will perform an additional validation check from a trusted list of "whitelisted" addresses in `depositaddresses.json` file. Withdraw will not be processed if destination wallet is not present in the file.                                                                                                                                                                                          |

##### Note
`updateFrequency` is itself a json document containing all duration values as query frequencies. 
See [ExchangeInfo default file](src/objects/src/exchangeinfodefault.hpp) as an example for the syntax.