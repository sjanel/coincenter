# coincenter

[![docker](https://github.com/sjanel/coincenter/actions/workflows/docker.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/docker.yml)
[![macos](https://github.com/sjanel/coincenter/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/macos.yml)
[![ubuntu](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml)
[![windows](https://github.com/sjanel/coincenter/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/windows.yml)

[![formatted](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/sjanel/coincenter/main/LICENSE)
[![GitHub Releases](https://img.shields.io/github/release/sjanel/coincenter.svg)](https://github.com/sjanel/coincenter/releases)

Command Line Interface (CLI) / library centralizing several crypto currencies exchanges REST API into a single, ergonomic all in one tool with a unified interface.

Main features:

## Market Data

- Currencies
- Markets
- Currency conversions
- Ticker
- Orderbook
- Traded volume
- Last price
- Last trades
- Withdraw fees

## Account requests

- Balance
- Trade (buy & sell in several flavors)
- Deposit information (address & tag)
- Recent Deposits
- Recent Withdraws
- Closed orders
- Opened orders
- Cancel opened orders
- Withdraw (with check at destination that funds are well received)
- Dust sweeper
  
## Supported exchanges

| Exchange |                                      Link                                       |
| -------- | :-----------------------------------------------------------------------------: |
| Binance  | [<img src="./resources/binancelogo.svg" width="170">](https://www.binance.com/) |
| Bithumb  | [<img src="./resources/bithumblogo.svg" width="55">](https://www.bithumb.com/)  |
| Huobi    |   [<img src="./resources/huobilogo.svg" width="160">](https://www.huobi.com/)   |
| Kraken   |  [<img src="./resources/krakenlogo.svg" width="90">](https://www.kraken.com/)   |
| Kucoin   |  [<img src="./resources/kucoinlogo.svg" width="150">](https://www.kucoin.com/)  |
| Upbit    |   [<img src="./resources/upbitlogo.svg" width="135">](https://www.upbit.com/)   |

<details><summary>Sections</summary>
<p>

- [coincenter](#coincenter)
  - [Market Data](#market-data)
  - [Account requests](#account-requests)
  - [Supported exchanges](#supported-exchanges)
  - [About](#about)
  - [Installation](#installation)
  - [Configuration](#configuration)
  - [Usage](#usage)
    - [Input / output](#input--output)
      - [Format](#format)
        - [Simple usage](#simple-usage)
        - [Multiple commands](#multiple-commands)
        - [Piping commands](#piping-commands)
        - [Repeat option](#repeat-option)
        - [Interrupt signal handling for graceful shutdown](#interrupt-signal-handling-for-graceful-shutdown)
      - [Logging](#logging)
        - [Activity history](#activity-history)
    - [Parallel requests](#parallel-requests)
    - [Public requests](#public-requests)
      - [Health check](#health-check)
      - [Currencies](#currencies)
        - [Examples](#examples)
      - [Markets](#markets)
        - [Examples of markets queries](#examples-of-markets-queries)
      - [Ticker information](#ticker-information)
      - [Order books](#order-books)
      - [Last 24h traded volume](#last-24h-traded-volume)
      - [Last price](#last-price)
      - [Last trades](#last-trades)
      - [Conversion](#conversion)
        - [Example: Print the value of 1000 Shiba Inu expressed in Solana on all exchanges where such conversion is possible](#example-print-the-value-of-1000-shiba-inu-expressed-in-solana-on-all-exchanges-where-such-conversion-is-possible)
        - [Example: Print the value of 1000 EUR expressed in Ethereum on all exchanges where such conversion is possible](#example-print-the-value-of-1000-eur-expressed-in-ethereum-on-all-exchanges-where-such-conversion-is-possible)
      - [Conversion path](#conversion-path)
        - [Example: Print the fastest conversion path from Shiba Inu to Solana on all exchanges where such conversion is possible](#example-print-the-fastest-conversion-path-from-shiba-inu-to-solana-on-all-exchanges-where-such-conversion-is-possible)
      - [Withdraw fees](#withdraw-fees)
        - [Example 1: query all withdraw fees](#example-1-query-all-withdraw-fees)
        - [Example 2: query ETH withdraw fees on Kraken and Kucoin](#example-2-query-eth-withdraw-fees-on-kraken-and-kucoin)
    - [Private requests](#private-requests)
      - [Selecting private keys on exchanges](#selecting-private-keys-on-exchanges)
      - [Balance](#balance)
      - [Trade](#trade)
        - [Syntax](#syntax)
          - [Standard - full information](#standard---full-information)
          - [With information from previous 'piped' command](#with-information-from-previous-piped-command)
        - [Options](#options)
          - [Trade timeout](#trade-timeout)
          - [Trade asynchronous mode](#trade-asynchronous-mode)
          - [Trade Price Strategy](#trade-price-strategy)
          - [Trade Price Picker](#trade-price-picker)
        - [Trade all](#trade-all)
        - [Examples with explanation](#examples-with-explanation)
      - [Multi Trade](#multi-trade)
        - [Trade simulation](#trade-simulation)
      - [Buy / Sell](#buy--sell)
        - [Syntax](#syntax-1)
          - [Standard - full information](#standard---full-information-1)
          - [Sell - with information from previous 'piped' command](#sell---with-information-from-previous-piped-command)
        - [Behavior](#behavior)
        - [Examples with explanation](#examples-with-explanation-1)
      - [Deposit information](#deposit-information)
      - [Recent deposits](#recent-deposits)
        - [Examples](#examples-1)
      - [Recent withdraws](#recent-withdraws)
        - [Examples](#examples-2)
      - [Closed orders](#closed-orders)
      - [Opened orders](#opened-orders)
      - [Cancel opened orders](#cancel-opened-orders)
      - [Withdraw coin](#withdraw-coin)
        - [Withdraw options](#withdraw-options)
          - [Withdraw refresh time](#withdraw-refresh-time)
          - [Withdraw asynchronous mode](#withdraw-asynchronous-mode)
      - [Dust sweeper](#dust-sweeper)
        - [Syntax](#syntax-2)
          - [Standard - full information](#standard---full-information-2)
          - [Sell - with information from previous 'piped' command](#sell---with-information-from-previous-piped-command-1)
    - [Monitoring options](#monitoring-options)
    - [Limitations](#limitations)
    - [Examples of use cases](#examples-of-use-cases)
      - [Get an overview of your portfolio in Korean Won](#get-an-overview-of-your-portfolio-in-korean-won)
      - [Trade 1000 euros to XRP on kraken with a maker strategy](#trade-1000-euros-to-xrp-on-kraken-with-a-maker-strategy)
      - [Trade 1000 euros to XRP on kraken with a maker strategy in simulation mode](#trade-1000-euros-to-xrp-on-kraken-with-a-maker-strategy-in-simulation-mode)
      - [Prints conversion paths](#prints-conversion-paths)
      - [Prints all markets trading Stellar (XLM)](#prints-all-markets-trading-stellar-xlm)
      - [Prints bithumb and upbit orderbook of depth 5 of Ethereum and adds a column conversion in euros](#prints-bithumb-and-upbit-orderbook-of-depth-5-of-ethereum-and-adds-a-column-conversion-in-euros)
      - [Prints last 24h traded volume for all exchanges supporting ETH-USDT market](#prints-last-24h-traded-volume-for-all-exchanges-supporting-eth-usdt-market)
      - [Prints last price of Cardano in Bitcoin for all exchanges supporting it](#prints-last-price-of-cardano-in-bitcoin-for-all-exchanges-supporting-it)
      - [Prints value of `1000 XML` into SHIB for all exchanges where such conversion is possible](#prints-value-of-1000-xml-into-shib-for-all-exchanges-where-such-conversion-is-possible)

</p>
</details>

## About

`coincenter` is not just another random implementation of some exchanges' REST API, it also provides a **higher abstraction** of each platform specificities by providing workarounds for the ones not implementing some queries (for instance, *withdrawal fees* is rarely accessible with the public endpoint, but `coincenter` can handle their query for all exchanges, by using screen scraping techniques of external resources when needed).

Also, it **takes the user by the hand** for more complex queries, like **trades** and **withdraws**, working by default in **synchronized** mode until the operation is finished (*withdraw* will not finish until the funds are **received** and **available** at destination, *trade* will not finish until the opened order is either **matched / canceled**).

Technically speaking, this project is for **C++ enthusiasts** providing an alternative to other crypto exchanges clients often written in higher level languages. It is an illustration of the elegance and possibilities of modern **C++** in an area where it is (sadly) barely chosen.

Of course, all suggestions to improve the project are welcome (should it be bug fixing, support of a new crypto exchange, feature addition / improvements or even technical aspects about the source code and best development practices).

## Installation

See [INSTALL.md](INSTALL.md)

## Configuration

See [CONFIG.md](CONFIG.md)

## Usage

### Input / output

#### Format

##### Simple usage

`coincenter` is a command line tool with ergonomic and easy to remember option schema. You will usually provide this kind of input:

- Command name, without hyphen (check what are the available ones with `help`)
- Followed by either:
  - Nothing, meaning that command will be applied globally, when applicable
  - Amount with currency or any currency, separated by dash to specify pairs, or source - destination
  - Comma separated list of exchanges (all are considered if not provided)

Example:

```bash
                  Ori curr.   exchange1
                      |          |
coincenter trade 0.05BTC-USDT,kraken,huobi
                  |        |           |
             from amt.  to curr.    exchange2
```

By default, result of command is printed in a formatted table on standard output.
You can also choose a *json* output format with option `-o json`.

##### Multiple commands

It is possible to give multiple commands that are to be executed sequentially. A new command name indicates the beginning of a new command.

For instance:

```bash
         1st command                      2nd command options
              |                               /       \
coincenter balance orderbook XRP-USDT,binance --cur EUR
                   \                        /
                           2nd command
```

##### Piping commands

Some commands' input can be deduced from previous commands' output, a bit like piping commands in Linux works.
Input commands accepting previous commands' output are:

- Withdraw
- Trade
- Sell

For example:

```bash
               1st command                  3rd command
           /                 \                 /  \
coincenter buy 1500XLM,binance withdraw kraken sell
                               \             /
                                 2nd command
```

The 1500XLM will be considered for withdraw from Binance if the buy is completed, and the XLM arrived on Kraken considered for selling when the withdraw completes.

##### Repeat option

`coincenter` commands are normally executed only once, and program is terminated after it.
To continuously query the same option use `-r <[n]>` or `--repeat <[n]>` (the `n` integer is optional) to repeat `n` times the given command(s) (or undefinably if no `n` given).

**Warning**: for trades and withdraw commands, use with care.

Between each repeat you can set a waiting time with `--repeat-time` option which expects a time duration.

It can be useful to store logs for an extended period of time and for [monitoring](#monitoring-options) data export purposes.

##### Interrupt signal handling for graceful shutdown

`coincenter` can exit gracefully with `SIGINT` and `SIGTERM` signals. When it receives such a signal, `coincenter` will stop processing commands after current one (ignoring the [repeat](#repeat-option) as well).

This allows to flush correctly the latest data (caches, logs, etc) to files at termination.

#### Logging

`coincenter` uses [spdlog](https://github.com/gabime/spdlog) for logging.

`spdlog` is set up asynchronously, and it's possible to log in rotating files in addition of the console, with different levels for each.

By default, it logs in the console with the 'info' level, and in 'debug' in rotating files.

For this, you can configure statically the default level for each in the [config file](CONFIG.md#general-options-description).
It can be overridden on the command line, with options `--log <log-level>` and `--log-file <log-level>`.

##### Activity history

It is also possible to store relevant commands results (in `data/log/activity_history_YYYY-MM.txt` files) to keep track of the most important commands of `coincenter`.
Each time a command of type present in the user defined list in `log.activityTracking.commandTypes` from `generalconfig.json` file is finished, its result is appended in json format to the corresponding activity history file of the current year and month.

This is useful for instance to keep track of all trades performed and can be later used for analytics of past performance gains / losses.

By default, it stores all types of trades and withdraws results, but the list is configurable to follow your needs.

**Important**: those files contain important information so `coincenter` does not clean them automatically. You can move the old activity history files if they take to much space after a while.

### Parallel requests

You may want to query several exchanges for a command at the same time. In this case, it's possible to ask `coincenter` to launch requests in parallel when it's possible.
By default, the number of requests in parallel is `1`. To increase it, change the value of the field `nbMaxParallelRequests` in `generalconfig.json` file (more information [here](CONFIG.md#general-options-description)).

You will have a nice boost of speed when you query the same thing from multiple exchanges / or accounts. However, the logs may not be ordered anymore.

### Public requests

#### Health check

`health-check` pings the exchanges and checks if there are up and running.
It is the first thing that is checked in exchanges unit tests, hence if the health check fails for an exchange, the tests are skipped for the exchange.

#### Currencies

`currencies` command aggregates currency codes for given list of exchanges. It also tells for which exchanges it can be deposited to, withdrawn from, and if it is a fiat money.

##### Examples

List all currencies for all supported exchanges

```bash
coincenter currencies
```

List all currencies for kucoin and upbit

```bash
coincenter currencies kucoin,upbit
```

#### Markets

Use the `markets` command to list markets. This is useful to check how you can trade your coins.
It takes an optional combination of a maximum of two currencies:

- if none is specified, all markets are returned
- if only one is specified, all markets trading the currency will be returned
- if two currencies are specified (should be separated with `-`), only exchanges listing given market will be returned

Also, result can be narrowed to list of exchanges given after the optional currencies. If no exchanges are provided, `coincenter` will simply query all supported exchanges and list the markets involving the given currencies if they exist.

**Note**: Markets are returned with the currency pair presented in original order from the exchange, as it could give additional information for services relying on this option (even though it's not needed for `trade` option of `coincenter`)

##### Examples of markets queries

Lists all markets for all exchanges

```bash
coincenter markets
```

List all markets involving Ethereum in Huobi

```bash
coincenter markets eth,huobi
```

List exchanges where the pair AVAX-USDT is supported

```bash
coincenter markets avax-usdt
```

#### Ticker information

Retrieve ticker information for all markets of one, several or all exchanges with `ticker [exchange1,exchange2,...]` option.
List of exchanges should be given as lower case, comma separated. But it is optional, all exchanges are considered if not provided.

Example: Print ticker information for kraken and huobi exchanges

```bash
coincenter ticker kraken,huobi
```

#### Order books

Check market order books of a currency pair with `orderbook` option. By default, chosen `depth` is `10`, can be configured with `--depth`.
Similarly to `ticker` option, exchanges list is optional. If not provided, all exchanges will be queried and only the market order books from exchange with proposes this market are returned.

In addition, for convenience, you can also specify a currency in which to convert the price if you are more familiar with it. For instance, you want to print the order book of BTC-KRW in Bithumb exchange, but as you are not familiar with Korean won, you want to convert to USD as well. `--cur usd` is for this purpose.

Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance

```bash
coincenter orderbook ada-usdt,kraken,binance --depth 20
```

#### Last 24h traded volume

Fast query last 24h traded volume with `volume-day` option on one market on one, several or all exchanges (as usual, see above options).

Example: Print last 24h traded volume on market XLM-BTC for all exchanges supporting it

```bash
coincenter volume-day xlm-btc
```

#### Last price

Fast query last traded price with `price` option on one market on one, several or all exchanges (as usual, see above options).

Example: Print last price on market SOL-BTC for all exchanges supporting it

```bash
coincenter price sol-btc
```

#### Last trades

Get a sorted list of last trades with `last-trades` option on one market on one, several or all exchanges.
You can specify the number of last trades to query (for exchanges supporting this option) with `--n`.

Example: Print the last 15 trades on DOT-USDT on Binance and Huobi

```bash
coincenter last-trades dot-usdt,binance,huobi --n 15
```

#### Conversion

From a starting amount to a destination currency, `conversion` examines the shortest conversion path (in terms of number of conversion) possible to reach the destination currency, on optional list of exchanges, and return the converted amount on each exchange where such conversion is possible. The trade fees **are** taken into account for this conversion.

The conversion path chosen is the fastest (in terms of number of markets, so trades) possible.

**Important note**: fiat conversions (including fiat-like crypto-currencies like `USDT`) are allowed even if the corresponding market is not an exchange market only as a first or a last conversion. No trade fees are taken into account for fiat conversions.

##### Example: Print the value of 1000 Shiba Inu expressed in Solana on all exchanges where such conversion is possible

```bash
coincenter conversion 1000shib-sol
```

Possible output:

```bash
+----------+------------------------------+
| Exchange | 1000 SHIB converted into SOL |
+----------+------------------------------+
| binance  | 0.00020324242724052 SOL      |
| bithumb  | 0.00020297245011085 SOL      |
| huobi    | 0.00020276144764053 SOL      |
| kraken   | 0.00020323457500308 SOL      |
| kucoin   | 0.00020338540268269 SOL      |
| upbit    | 0.00020302426059262 SOL      |
+----------+------------------------------+
```

##### Example: Print the value of 1000 EUR expressed in Ethereum on all exchanges where such conversion is possible

```bash
coincenter conversion 1000eur-eth
```

Possible output:

```bash
+----------+-----------------------------+
| Exchange | 1000 EUR converted into ETH |
+----------+-----------------------------+
| binance  | 0.31121398375705967 ETH     |
| bithumb  | 0.29715335705445442 ETH     |
| huobi    | 0.30978082796054002 ETH     |
| kraken   | 0.3110542008206231 ETH      |
| kucoin   | 0.31194281985067939 ETH     |
| upbit    | 0.29672491761070147 ETH     |
+----------+-----------------------------+
```

#### Conversion path

This option is similar to previous one ([conversion](#conversion)) but takes a starting currency code instead of an amount to convert, and only real markets from exchanges are taken into account (in particular, fiat conversions that are not proposed by the exchange as a market are not possible).

It will print the result as an ordered list of markets for each exchange.

Option `conversion-path` is used internally for [multi trade](#multi-trade) but is provided as a stand-alone query for information.

##### Example: Print the fastest conversion path from Shiba Inu to Solana on all exchanges where such conversion is possible

```bash
coincenter conversion-path shib-sol
```

Possible output:

```bash
+----------+--------------------------------------+
| Exchange | Fastest conversion path for SHIB-SOL |
+----------+--------------------------------------+
| binance  | SHIB-FDUSD,SOL-FDUSD                 |
| bithumb  | SHIB-KRW,SOL-KRW                     |
| huobi    | SHIB-USDT,SOL-USDT                   |
| kraken   | SHIB-USDT,SOL-USDT                   |
| kucoin   | SHIB-USDC,SOL-USDC                   |
| upbit    | SHIB-KRW,SOL-KRW                     |
+----------+--------------------------------------+
```

**Note**: when several conversion paths of same length are possible, it will favor the ones not involving fiat currencies (stable coins are **not** considered as fiat currencies).

#### Withdraw fees

Some exchanges provide a withdraw fees endpoint. Some propose it without an API Key, other require an API Key. `coincenter` uses other sources of data (although less reliable) for exchanges which do not propose withdraw fees.

You can query all withdraw fees at once, for all exchanges or provided ones. You can also specify a currency to filter fees only for this currency.

##### Example 1: query all withdraw fees

```bash
coincenter withdraw-fees
```

##### Example 2: query ETH withdraw fees on Kraken and Kucoin

```bash
coincenter withdraw-fees eth,kraken,kucoin
```

### Private requests

These requests will require that you have your secret keys in `data/secret/secret.json` file, for each exchange used.
You can check easily that it works correctly with the `balance` option.

#### Selecting private keys on exchanges

`coincenter` supports several keys per exchange. Here are both ways to target exchange(s) for `coincenter`:

- **`exchange`** matches **all** keys on exchange `exchange`
- **`exchange_keyname`** matches **only** `keyname` on exchange `exchange`
  
All options use this pattern, but they will behave differently if several keys are matched for one exchange. For instance:

- Balance will sum all balance for matching accounts
- Trade and multi trade will share the amount to be trade across the matching exchanges, depending on the amount available on each (see [trade](#trade)).

Example:

Let's say you have `jack` and `joe` accounts (the name of the keys in `secret.json` file) for `kraken`:

```json
{
  "kraken": {
    "jack": {
      "key": "...",
      "private": "..."
    },
    "joe": {
      "key": "...",
      "private": "..."
    }
  }
}
```

When you need to specify one key, you can suffix `jack` or `joe` after the exchange name `kraken`: `kraken_joe`.

| Command                                         | Explanation                                                                                                           |
| ----------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `coincenter balance kraken`                     | Sum of balances of 'jack' and 'joe' accounts of kraken                                                                |
| `coincenter balance kraken_jack`                | Only balance of 'jack' account of kraken                                                                              |
| `coincenter trade 1000usdt-sol,kraken`          | `coincenter` will trade a total of 1000 USDT from 'jack' and/or 'joe' (see [trade](#trade))                           |
| `coincenter trade 1000usdt-sol,kraken_joe`      | Perform the trade on 'joe' account only                                                                               |
|                                                 |
| `coincenter withdraw-apply 1btc,kraken-binance` | <span style="color:red">**Error**</span>: `coincenter` does not know if it should choose 'jack' or 'joe' for withdraw |

If you have only one key per exchange, suffixing with the name is not necessary for **all** commands (but supported):

```json
{
  "binance": {
    "averell": {
      "key": "...",
      "private": "..."
    }
  }
}
```

| Command                                         | Explanation                                              |
| ----------------------------------------------- | -------------------------------------------------------- |
| `coincenter balance binance`                    | Only one account in `binance`, this will print `averell` |
| `coincenter balance binance_averell`            | Same as above                                            |
| `coincenter trade 1000usdt-sol,binance`         | **OK** as no ambiguity                                   |
| `coincenter trade 1000usdt-sol,binance_averell` | **OK** as well                                           |

#### Balance

Check your available balance across supported exchanges at one glance!

```bash
coincenter balance
```

prints a formatted table with sum of assets from all loaded private keys (for all exchanges).
It is also possible to give a list of exchanges (comma separated) to print balance only on those ones.

You can also specify a currency to which all assets will be converted (if possible) to have a nice estimation of your total balance in your currency.
The currency acronym should be at the first position of the comma separated values, the next ones being the accounts.
For instance, to print total balance on Kraken and Bithumb exchanges, with a summary currency of *Euro*:

```bash
coincenter balance eur,kraken,bithumb
```

By default, the balance displayed is the available one, with opened orders remaining unmatched amounts deduced.
To include balance in use as well, `--in-use` should be added.

#### Trade

A trade is a request to change a certain amount in a given currency into another currency on a list of considered exchanges (or all if none specified).
It can be tailored with a range of options:

- Absolute start amount or percentage of available amount
- [Single / multi trade](#multi-trade)
- [Automatic price chooser strategy](#trade-price-strategy)
- [Custom price](#trade-price-picker)
- [Timeout](#trade-timeout)
- [Asynchronous mode](#trade-asynchronous-mode)
- Smart multi account behavior (see below)

Similarly to other options, list of exchanges is optional.
If empty, all exchanges having some balance for given start amount may be considered.
When several accounts are involved, the trade are performed on the exchanges providing the most available starting amount to trade in priority, and are selected until total available reaches the desired one. This behavior ensures the minimum number of independent trades to reach the desired goal.
Check the [examples](#examples-with-explanation) below for more precise information.

Given start amount can be absolute, or relative with the `%` character. In the latter case, the total start amount will be computed from the total available amounts of the matching exchanges. This implies a call to `balance` on the considered exchanges before the actual trade. Percentages should not be larger than `100` and may be decimal numbers.

If only one private exchange is given and start amount is absolute, `coincenter` will not query your funds prior to the trade to minimize response time, make sure that inputs are correct or program may throw an exception.

##### Syntax

A trade has two input flavors.

###### Standard - full information

This is the most common trade input. You will need to provide, in order:

- decimal, positive amount, absolute (default) or relative (percentage suffixed with `%`)
- first currency linked to this amount
- a dash `-` to help parser split the two currencies
- second currency destination of the wished trade
- optional list of exchanges where to perform the trade (if several accounts are matched, the amount will be split among them, with a designated strategy)

```bash
coincenter trade 35.5%ETH-USDT,kucoin
```

###### With information from previous 'piped' command

This second flavor can be useful for piped commands. The unique trade input argument will be the *destination currency*. The amount and considered exchanges will be deduced from the output of the previous command.

Example:

You withdraw a certain amount of ETH from exchange kraken to kucoin. You want to trade the received amount on kucoin to USDT:

```bash
coincenter withdraw-apply 1.45ETH,kraken-kucoin trade USDT
```

If the previous command is a withdraw, input exchange list is a single one.

Note that trades themselves can be piped, as the output has an amount and list of exchanges.

Trade 15 % of my all available BTC to USDT (whatever the exchanges), and trade all the resulted USDT to ETH:

```bash
coincenter trade 15%BTC-USDT trade ETH
```

**Important note**: the piped command will be possible only if the previous command is *complete*. For a trade (or buy / sell), a successful command implies that all the initial amount has been traded from, that no remaining unmatched amount is left. For a withdraw, it means that it successfully arrived to the destination exchange.

##### Options

###### Trade timeout

A trade is **synchronous** by default with the created order of `coincenter` (it follows the lifetime of the order).
The trade ends when one of the following events occur:

- Order is matched completely
- Order has still some unmatched amount when trade reaches the **timeout**

By default, the trade timeout is set to **30 seconds*. If you wish to change it, you can use `--timeout` option. It expects a time and supports all the common time suffixes `y` (years), `mon` (months), `w` (weeks), `d` (days), `h` (hours), `min` (minutes), `s` (seconds), `ms` (milliseconds) and `us` (microseconds).

Value can contain several units, but do not support decimal values. For instance, use `2min30s` instead of `2.5min`.

Another example: `--timeout 3min30s504ms`

###### Trade asynchronous mode

Above option allows to control the full life time of a trade until it is either fully matched or cancelled (at timeout).
If you want to only **fire and forget** an order placement, you can use `--async` flag.
It will exit the place process as soon as the order is placed, before any query of order information, hence it's the fastest way to place an order.

Note that it's not equivalent of choosing a **zero** trade timeout (above option), here are the differences:

- Order is not cancelled after the trade process in asynchronous mode, whereas it's either cancelled or matched directly in synchronous mode
- Asynchronous trade is faster as it does not even query order information

###### Trade Price Strategy

Possible order price strategies, configurable with `--price-strategy`:

- `maker`: order placed at limit price and regularly updated to limit price (default)
- `taker`: order placed at market price, should be matched immediately
- `nibble`: order price continuously set at limit price + (buy)/- (sell) 1. This is a hybrid mode between the above two methods, ensuring continuous partial matches of the order over time, but at a controlled price (market price can be dangerously low for some short periods of time with large sells)

Use command line option `trade` to make a trade from a departure amount.

###### Trade Price Picker

In order to control more precisely the price of the order, `coincenter` supports custom price as well thanks to `--price` option. Note that this is not compatible with above **trade price strategy** option.
`--price` expects either an integer (!= 0) representing a **relative** price compared to the limit price, or a monetary amount representing an **absolute** fixed price (decimal amount with the price currency).
In fact, parser will recognize a relative price if the amount is without any decimals and without any currency. The price currency will be overriden by the engine, but you can add it for readability and to remove ambiguity of an integer price for the parser.

| `--price` value examples | Explanation                                                                                                                       |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------------- |
| `35`                     | *relative* price                                                                                                                  |
| `-2`                     | *relative* price                                                                                                                  |
| `34.6`                   | *absolute* price (engine will take the currency from the market of the trade)                                                     |
| `25 XXX`                 | *absolute* price (engine will override the currency from the market of the trade and not consider `XXX`, no error will be raised) |

####### Absolute price

When requesting an absolute price, the order will be placed exactly at this price. Depending on the order book and the limit prices, order may or may not be matched instantly. Double check the price before launching your trade command!

**Notes**:

- Such an order will not be compatible with [multi trade](#multi-trade) because an absolute price makes sense only for a specific market. However, if you ask a multi trade with a fixed price, no error will be raised, and `coincenter` will simply launch a single trade.
- Order price will not be continuously updated over time

####### Relative price

The **relative price** makes it possible to easily settle a price anywhere in the orderbook. The chosen price is **relative** to the order book limit price - it represents how far away the price is related to the 'center' prices. The higher (or lower) the value, the more difficult it will be for the order to be bought - or sold.

A positive value indicates a higher price for a sell, and a lower price for a buy, resulting in a safe position that should not be matched immediately. A negative value is the opposite - which will most probably be matched immediately, depending on the amount of the order.

So, choosing a relative price of `1`: `--price 1` is equivalent as the `maker` trade price strategy, and choosing a relative price of `-1` is equivalent to the `nibble` strategy.

The relative price is, contrary to the **absolute** (fixed) price above, compatible with multi-trade. Also, it is continuously updated over time depending on the `--update-price` option.

Here is a visual example, considering an order book of market A-B:

| Sellers of A (buying B) asks | A price in B | Buyers of A (selling B) bids | Buy relative price | Sell relative price |
| ---------------------------- | ------------ | ---------------------------- | ------------------ | ------------------- |
| 13                           | 0.50         |                              | **-5**             | **5**               |
| 11                           | 0.49         |                              | **-4**             | **4**               |
| 9                            | 0.47         |                              | **-3**             | **3**               |
| 5                            | 0.46         |                              | **-2**             | **2**               |
| 17                           | 0.45         | <- **lowest ask price**      | **-1**  (nibble)   | **1**  (maker)      |
| **highest bid price** ->     | 0.44         | 2                            | **1**  (maker)     | **-1**  (nibble)    |
|                              | 0.43         | 1.5                          | **2**              | **-2**              |
|                              | 0.42         | 15                           | **3**              | **-3**              |
|                              | 0.41         | 73                           | **4**              | **-4**              |
|                              | 0.39         | 9                            | **5**              | **-5**              |
|                              | 0.35         | 2.56                         | **6**              | **-6**              |
|                              | 0.34         | 48                           | **7**              | **-7**              |
|                              | 0.33         | 8                            | **8**              | **-8**              |

Say you want to place a buy order of *A* with n *B* at relative price **3** for instance:

```bash
coincenter trade nB-A --price 3
```

The chosen price will be `0.42`.

##### Trade all

Instead of providing a start amount to trade, specify a currency. So `trade-all XXX...` is equivalent to `trade 100%XXX`.

Example: Trade all available EUR on all exchanges to BTC

```bash
coincenter trade-all EUR-BTC
```

##### Examples with explanation

```bash
coincenter trade 1btc-usdt
```

Trade 1 BTC to USDT on all exchanges supporting BTC-USDT market and having some available BTC.
| Exchange        | Amount  | Considered amount for trade |
| --------------- | ------- | --------------------------- |
| exchange1_user1 | 0.1 BTC | 0 BTC                       |
| exchange1_user2 | 0.2 BTC | **0.1 BTC**                 |
| exchange2_user1 | 0.3 BTC | **0.3 BTC**                 |
| exchange3_user1 | 0.6 BTC | **0.6 BTC**                 |
| exchange4_user1 | 0 BTC   | 0 BTC                       |

```bash
coincenter trade 66.6%xlm-btc,kraken,huobi
```

Among all available XLM amount on all accounts on Kraken and Huobi, launch trades with a total of 66.6 % of starting XLM.
| Exchange      | Amount      | Considered amount for trade |
| ------------- | ----------- | --------------------------- |
| kraken_user1  | **50 XLM**  | 0 XLM                       |
| kraken_user2  | **250 XLM** | **99.6 XLM**                |
| huobi_user1   | **300 XLM** | **300 XLM**                 |
| bithumb_user1 | 100 XLM     |                             |
| upbit_user1   | 15000 XLM   |                             |

There is a total of `600` XLM available on all accounts of Kraken and Huobi, trading a total of 66.6 % of them: `399.6` XLM.
Prioritize the accounts with the largest amount of XLM first.

```bash
coincenter trade 0.5btc-eur,kraken --sim --strategy nibble --timeout 1min
```

Trade 0.5 BTC to euros on all Kraken accounts, in simulated mode (no real order will be placed, useful for tests), with the 'nibble' strategy, an emergency mode triggered before 2 seconds of the timeout of 1 minute

```bash
coincenter trade-all doge-usdt,binance_user1,huobi --strategy taker
```

Trade all DOGE to USDT on Binance account with key name 'user1' and all Huobi accounts, with the 'taker' strategy, keeping default time out and cancel remaining untouched amount when trade expires

You can also choose the behavior of the trade when timeout is reached, thanks to `--timeout-match` option. By default, when the trade expires, the last unmatched order is cancelled. With above option, instead it places a last order at market price (should be matched immediately).

#### Multi Trade

If you want to trade coin *AAA* into *CCC* but exchange does not have a *AAA-CCC* market and have *AAA-BBB* and *BBB-CCC*, then it's possible with a multi trade, which will perform a series of trades in order until the destination currency is reached. Other trade options are the same than for a simple trade. `coincenter` starts by evaluating the shortest conversion path to reach *CCC* from *AAA* and then applies the single trades in the correct order to its final goal.

In order to activate multi trades, there are two ways:

- Default behavior is controlled by [multiTradeAllowedByDefault option](CONFIG.md#exchanges-options-description) in the `exchangeconfig.json` file. If `true`, multi trade is allowed for the given exchange (or all exchanges if under `default` level).
- It can be forced by adding command line flag `--multi-trade`.

In the case it's activated in the config file, `--no-multi-trade` can force deactivation of the multi trade.

##### Trade simulation

Some exchanges (**Kraken** and **Binance** for instance) allow to actually query their REST API in simulation mode to validate the query and not perform the trade. It is possible to do this with `--sim` option. For exchanges which do not support this validation mode, `coincenter` will simply directly finish the trade entirely (taking fees into account) ignoring the trade strategy.

#### Buy / Sell

Trade family of commands require that you specify the *start amount* (with the start currency) and the *target currency*. You can use `buy` and `sell` commands when you have a start amount (for *sell*) or a target amount (for *buy*) only. It's more easy to use, but `coincenter` needs to know which are the [preferred currencies](CONFIG.md#exchanges-options-description) to which it can *sell* the start amount to, or use as start amount for a *buy*.

Sell option also supports percentage as start amount. In this case, the desired percentage of total available amount of given currency on matching exchanges (the ones specified after the `,` or all if none given as usual) will be sold. To complement this, `sell-all` option with a start currency instead of an amount is supported as well, which is syntactic sugar of a sell of `100%` of available amount.

Buy a percentage is not available yet, simply because I am not sure about what should be the behavior of this option. If you have ideas about it, do not hesitate to propose them.

The list of preferred currencies should be filled prior to **buy / sell** command and is statically loaded at start of coincenter. It is an array of currencies ordered by decreasing priority, and they represent the currencies that can be used as target currency for a *sell*, and base currency for a *buy*. See [config](CONFIG.md#exchanges-options-description) file to see how to set the preferred currencies.

##### Syntax

A sell has two input flavors, whereas a buy only one.

###### Standard - full information

This is the most common buy/sell input. You will need to provide, in order:

- decimal, positive amount, absolute (for buy and sell, default) or relative for a sell (percentage suffixed with `%`, incompatible with buy)
- currency linked to this amount
- optional list of exchanges where to perform the trade (if several accounts are matched, the amount will be split among them, with a designated strategy)

Buy 500 SOL, considering all my accounts on Binance and Bithumb.

```bash
coincenter buy 500SOL,binance,bithumb
```

Sell 25 % of all my available ETH on all accounts

```bash
coincenter sell 25%ETH
```

###### Sell - with information from previous 'piped' command

This second flavor can be useful for piped commands, for *sell* only.
You do not need to provide any information to the `sell` command. The amount and considered exchanges will be deduced from the output of the previous command.

Examples:

You withdraw a certain amount of XRP from exchange upbit to bithumb. You want to sell the received amount of XRP on bithumb:

```bash
coincenter withdraw-apply 500XRP,upbit-bithumb sell
```

Note that trades themselves can be piped, as the output has an amount and list of exchanges.

Following example would be foolish, but possible by coincenter.

Buy 1 ETH on any exchange then sell it:

```bash
coincenter buy 1ETH sell
```

**Important note**: as for [trade](#with-information-from-previous-piped-command), the piped command will be possible only if the previous command is *complete*.

##### Behavior

- All currencies present in the **preferred currencies** of a given exchange may be used to validate and perform trades. For instance, if you decide to include non stable cryptos (for instance, `BTC` as it is often involves in many pairs), it can be used as a base for a *buy*, or target for a *sell*. Thus as a recommendation, if you do add such currencies as *preferred*, prefer placing them after the fiats and stable coins.
- Several trades may be performed on the same account for a **buy**. For instance: you have both `EUR` and `USDT` and they are both present in the *preferred currencies*, and the desired amount of target currency is high enough such that it would *eat* all of your `EUR` or `USDT`.
- Similarly to **trade** options, `coincenter` tries to minimize the number of actual trades to achieve the desired goal. So it will favor exchanges and currencies with the largest available amount.
- For the `buy`, trade fees are taken into account to attempt to reach exactly the desired **net** amount.

##### Examples with explanation

```bash
coincenter sell 1 BTC,exchange1,exchange3_user1
```

Sell `1 BTC` on all accounts on `exchange1` and `exchange3_user1` to any preferred currencies

| Exchange        | Amount  | Considered amount for sell | Preferred currencies | Markets              | Sold to  |
| --------------- | ------- | -------------------------- | -------------------- | -------------------- | -------- |
| exchange1_user1 | 0.1 BTC | **0.1 BTC**                | `[USDT,EUR]`         | **BTC-USDT**,BTC-EUR | **USDT** |
| exchange1_user2 | 0.2 BTC | **0.2 BTC**                | `[USDT,EUR]`         | **BTC-USDT**,BTC-EUR | **USDT** |
| exchange2_user1 | 0.3 BTC | 0 BTC                      |                      |                      |          |
| exchange3_user1 | 0.6 BTC | **0.6 BTC**                | `[USDT,KRW]`         | **BTC-KRW**          | **KRW**  |
| exchange4_user1 | 0 BTC   | 0 BTC                      |                      |                      |          |

A total of `0.9 BTC` will be sold, to `USDT` on `exchange1` and to `KRW` on `exchange3`.

```bash
coincenter buy 2.5ETH
```

Buy `2 ETH` on all accounts which has some available amount in preferred currencies.
Let's consider following prices, without any trade fees (`coincenter` takes trade fees into account for `buy`):

- 1 ETH = 1000 EUR
- 1 ETH = 1200 USDT
- 1 ETH = 1000000 KRW

| Exchange        | Amount            | Considered amount for buy | Preferred currencies | Markets              | Bought                    |
| --------------- | ----------------- | ------------------------- | -------------------- | -------------------- | ------------------------- |
| exchange1_user1 | 350 EUR           | 0 EUR                     | `[USDT,EUR]`         | ETH-USDT             | 0 ETH (no market ETH-EUR) |
| exchange1_user2 | 600 USDT          | **600 USDT**              | `[USDT,EUR]`         | **ETH-USDT**         | **0.5 ETH**               |
| exchange2_user1 | 100 EUR           | 0 EUR                     | `[USDT]`             |                      |                           |
| exchange3_user1 | 800 EUR, 300 USDT | **800 EUR, 240 USDT**     | `[USDT,KRW,EUR]`     | **ETH-EUR,ETH-USDT** | **0.8 + 0.20 ETH**        |
| exchange4_user1 | 500000 KRW        | **500000 KRW**            | `[USDT,KRW]`         | **ETH-KRW**          | **0.5 ETH**               |

A total of `2 ETH` will be bought, from **600 USDT** on `exchange1_user2`, **800 EUR** and **240 USDT** from `exchange3_user1` and **500000 KRW** from `exchange4_user1`. As you can see in this example, it's possible to make several trades per account for one `buy` request.

#### Deposit information

You can retrieve one deposit address and tag from your accounts on a given currency provided that the exchange can receive it with `deposit-info` option. It requires a currency code, and an optional list of exchanges (private or public names, see [Selecting private keys on exchanges](#selecting-private-keys-on-exchanges)). If no exchanges are given, program will query all accounts and retrieve the possible ones.

If for a given exchange account a deposit address is not created yet, `coincenter` will create one automatically.

Only one deposit address per currency / account is returned.

**Important note**: only addresses which are validated against the `depositaddresses.json` file will be returned (unless option `validateDepositAddressesInFile` is set to `false` in `static/exchangeconfig.json` file). This file allows you to control addresses to which `coincenter` can send funds, even if you have more deposit addresses already configured.

#### Recent deposits

To retrieve the most recent deposits, use the `deposits` option. Currency is optional - if not provided, deposits in all currencies will be returned.

Some exchanges do not provide a method to retrieve recent deposits for more than one currency (Kraken and Bithumb for instance).
If you request all currencies on these exchanges, `coincenter` will not query deposits for all existing currencies, but only for the currencies present in your balance to decrease the number of queries. Still, it's an heuristic so it may of course miss other deposits.

By default, it returns all deposits on given currency, or any currency. You can filter the deposits with options:

- `--max-age <time>` to set a maximum age for the deposits to retrieve
- `--min-age <time>` to set a minimum age for the deposits to retrieve
- `--id <id1,id2,...>` to retrieve only deposits with given IDs

##### Examples

`coincenter deposits kraken --max-age 2w`: retrieves all deposits of the last two weeks

`coincenter deposits eth --id myid1,myid2`: retrieves all deposits of Ethereum, if they have either id `myid1` or `myid2`

#### Recent withdraws

Similarly to deposits, you can query the recent **withdraws** with command `withdraws`. It works the same ways as deposits - you just need to replace `deposits` with `withdraws` in the option name (for instance, `--max-age` specifies the maximum age for the withdraws to retrieve).

##### Examples

Retrieve all recent withdraws from all accounts:

```bash
coincenter withdraws
```

Retrieve the withdraw of XRP on Upbit with id `myid3`

```bash
coincenter withdraws upbit,xrp --id myid3
```

#### Closed orders

Use option `orders-closed` to retrieve and print closed orders on your accounts. A closed order is an old order that has no remaining unmatched amount. You can provide only one currency code, two currency codes separated with a `-`, and / or a list of exchanges on which to filter the orders. It is possible to not specify anything as well, in this case, all closed orders will be returned, if the exchange provides a way to retrieve them all. In particular, a specific process is made for the following exchanges:

- On **Binance**, market is mandatory and if `orders-closed` query is made without the market, `coincenter` will log an error and return an empty result.
- On **Bithumb**, at least one currency code is needed. If only one currency code is given, `coincenter` will perform a closed orders query for all currencies that have a non-zero balance on the account.

Orders can be filtered according to placed time with options `--min-age` and `--max-age` specifying respectively the minimum and maximum age of the orders.

Finally, they can also be filtered by their ID, thanks to `--id` option. Give a list of comma separated string Ids.

Examples:

```bash
coincenter orders-closed --min-age 10min            # Retrieve all closed orders placed within the last 10 minutes
coincenter orders-closed kraken                     # Retrieve all closed orders on Kraken only
coincenter orders-closed eth --max-age 1w           # Retrieve all closed orders of at most 1 week old, on markets involving Ethereum on all exchanges
coincenter orders-closed xlm-usdt                   # Retrieve all closed orders matching markets USDT-XLM or XLM-USDT
coincenter orders-closed btc-krw,upbit,bithumb      # Retrieve all closed orders on BTC-KRW / KRW-BTC on Bithumb and Upbit
coincenter orders-closed binance_joe --id OID1,OID2 # Check if 'OID1' and 'OID2' are still closed on 'joe' account on Binance
```

#### Opened orders

Use option `orders-opened` to retrieve and print currently opened orders on your accounts. An opened order is an order that has some remaining unmatched amount.
The option syntax is the same as above [closed orders](#closed-orders).

#### Cancel opened orders

Opened orders can be cancelled with `orders-cancel` option, with the same filter arguments as opened orders option above.
**Beware**, no filtering options matches all opened orders, so this command:

```bash
coincenter orders-cancel
```

will cancel all opened orders on all available exchanges. You can first check the current open orders and order cancellations by ID:

```bash
coincenter orders-cancel --id OID1
```

cancels order Id OID1 only, on the exchange where it is found on (no error is raised if no such opened order has this ID).

#### Withdraw coin

It is possible to withdraw crypto currency with `coincenter` as well, in either a **synchronized** mode (withdraw will check that funds are well received at destination) or **asynchronous** mode, with command `withdraw-apply`.
Either an absolute amount can be specified, or a percentage (`10xrp` or `25%xrp` for instance). `withdraw-apply-all` is a convenient command wrapper of `withdraw-apply 100%`.

Some exchanges require that external addresses are validated prior to their usage in the API (*Kraken* and *Huobi* for instance).

To ensure maximum safety, there are two checks performed by `coincenter` prior to all withdraw launches:

- External address is not taken as an input parameter, but instead dynamically retrieved from the REST API `getDepositAddress` of the destination exchange (see [Deposit information](#deposit-information))
- By default (can be configured in `static/exchangeconfig.json` with boolean value `validateDepositAddressesInFile`) deposit address is validated in `<DataDir>/secret/depositaddresses.json` which serves as a *portfolio* of trusted addresses

Example: Withdraw 10000 XLM (Stellar) from Bithumb to Huobi:

```bash
coincenter withdraw-apply 10000xlm,bithumb-huobi
```

##### Withdraw options

###### Withdraw refresh time

You can customize the withdraw refresh time of the periodic checks during the continuous checking of withdraw status and receive at destination in **synchronous** mode.
For that, specify a different time period with `--refresh-time` that expects a string representation of a time.

Defaults to 5 seconds.

###### Withdraw asynchronous mode

By default `coincenter` will exit withdraw process only once destination has received the funds.
You can change the behavior to an **asynchronous** mode thanks to `--async` option, which is like a fire and forget mode. Once the withdraw is initiated, withdraw process is finished and the withdraw is not followed anymore.

#### Dust sweeper

If you are annoyed with this kind of balance:

```bash
| BTG      | 0.0000442                |
| DASH     | 0.00002264               |
| DOT      | 0.00009634               |
```

and you would like to "clean" these small amounts to zero, then *dust sweeper* is the option to use!

When user defines a threshold amount on a given currency, `dust-sweeper` can be used to attempt to sell all amount (if below this threshold) trying to reach exactly 0 amount on this currency for a clean balance.
The algorithm used by `coincenter` is a simple heuristic:

- it finds the best markets involving currency to be sold
- it tries to sell all immediately.
- if full selling is not possible, it buys a small amount of this currency
- retry until either maximum number of steps (configurable) is reached or balance is exactly 0 on given currency

Example: Attempts to clean amount of BTG on bithumb and upbit

```bash
coincenter dust-sweeper btg,bithumb,upbit
```

##### Syntax

Withdrawal input parameters can be provided in two flavors.

###### Standard - full information

This is the most common withdraw. You will need to provide, in order:

- decimal, positive amount, absolute (default) or relative (percentage suffixed with `%`)
- currency linked to this amount
- Exactly two exchange accounts separated by a dash '-'

###### Sell - with information from previous 'piped' command

This second flavor can be useful for piped commands.
If the previous command output implies only one exchange, then it will be considered the origin exchange for the withdraw.
The amount to be withdrawn is also taken from the output of previous command.
You will need to specify exactly one exchange which will be the destination of the withdrawal.

Examples:

You trade an amount of KRW on upbit into ADA, and the obtained ADA amount will be withdrawn to kraken:

```bash
coincenter trade 80%KRW-ADA,upbit withdraw-apply kraken
```

**Important note**: as for [trade](#with-information-from-previous-piped-command), the piped withdraw will be possible only if the previous trade is *complete*.

### Monitoring options

`coincenter` can export metrics to an external instance of `Prometheus` thanks to its implementation of [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) client. Refer to [Build with monitoring support](INSTALL.md#build-with-monitoring-support) section to know how to build `coincenter` with it.

Currently, its support is experimental and in development for all major options of `coincenter` (private and market data requests).
The metrics are exported in *push* mode to the gateway at each new query. You can configure the IP address, port, username and password (if any) thanks to command line options (refer to the help to see their names).

### Limitations

Be aware of the following limitations of `coincenter`:

- When several networks exist for a given currency, only the **main one** is considered.
  This is to ensure safety between withdrawals performed between exchanges.
- Only absolute withdraw fees are currently supported.
  In some cases (seen in Huobi), withdraw fee can be a percentage. They will be considered as 0.
- Not really a limitation of `coincenter` itself, but some **sensitive actions are not possible** by the exchanges API.
  For instance, withdrawal for Kraken is only possible for a stored destination made from the website first.

And probably more that I did not thought of, or never encountered. Feel free to open an issue and I will check it out if it's feasible!

### Examples of use cases

#### Get an overview of your portfolio in Korean Won

```bash
coincenter balance krw
```

#### Trade 1000 euros to XRP on kraken with a maker strategy

```bash
coincenter trade "1000eur-xrp,kraken" --strategy maker --timeout 60s
```

#### Trade 1000 euros to XRP on kraken with a maker strategy in simulation mode

```bash
coincenter trade "1000eur-xrp,kraken" --strategy maker --timeout 1min --sim
```

Possible output

```bash
+----------+-----------+----------+---------------------------+-------------------------+-----------+
| Exchange | Account   | From     | Traded from amount (real) | Traded to amount (real) | Status    |
+----------+-----------+----------+---------------------------+-------------------------+-----------+
| kraken   | testuser1 | 1000 EUR | 999.99999999954052 EUR    | 1221.7681748109101 XRP  | Partial   |
+----------+-----------+----------+---------------------------+-------------------------+-----------+
```

#### Prints conversion paths

How to change Yearn Finance to Bitcoin on all exchanges

```bash
coincenter conversion yfi-btc
```

Possible output

```bash
--------------------------------------------------
| Exchange | Fastest conversion path for YFI-BTC |
--------------------------------------------------
| binance  | YFI-BTC                             |
| bithumb  | YFI-KRW,BTC-KRW                     |
| huobi    | YFI-BTC                             |
| kraken   | YFI-BTC                             |
| kucoin   | YFI-USDT,BTC-USDT                   |
--------------------------------------------------
```

#### Prints all markets trading Stellar (XLM)

```bash
coincenter markets xlm
```

Possible output

```bash
-------------------------------
| Exchange | Markets with XLM |
-------------------------------
| binance  | XLM-BNB          |
| binance  | XLM-BTC          |
| binance  | XLM-BUSD         |
| binance  | XLM-ETH          |
| binance  | XLM-EUR          |
| binance  | XLM-TRY          |
| binance  | XLM-USDT         |
| bithumb  | XLM-KRW          |
| huobi    | XLM-BTC          |
| huobi    | XLM-ETH          |
| huobi    | XLM-HUSD         |
| huobi    | XLM-USDT         |
| kraken   | XLM-BTC          |
| kraken   | XLM-EUR          |
| upbit    | XLM-BTC          |
| upbit    | XLM-KRW          |
-------------------------------
```

#### Prints bithumb and upbit orderbook of depth 5 of Ethereum and adds a column conversion in euros

```bash
coincenter orderbook eth-krw,bithumb,upbit --cur eur --depth 5
```

Possible output

```bash
--------------------------------------------------------------------------------------
| Sellers of ETH (asks) | ETH price in KRW | ETH price in EUR | Buyers of ETH (bids) |
--------------------------------------------------------------------------------------
| 0.2673                | 3196000          | 2323.492         |                      |
| 9.7201                | 3195000          | 2322.765         |                      |
| 0.067                 | 3194000          | 2322.038         |                      |
| 12.3853               | 3193000          | 2321.311         |                      |
| 7.36                  | 3191000          | 2319.857         |                      |
|                       | 3189000          | 2318.403         | 0.015                |
|                       | 3188000          | 2317.676         | 2.0048               |
|                       | 3187000          | 2316.949         | 0.0628               |
|                       | 3186000          | 2316.222         | 9.9882               |
|                       | 3185000          | 2315.495         | 0.17                 |
--------------------------------------------------------------------------------------
[2021-05-25 17:57:35.939] [info] Order book of ETH-KRW on upbit requested with conversion rate 0.000727 EUR
--------------------------------------------------------------------------------------
| Sellers of ETH (asks) | ETH price in KRW | ETH price in EUR | Buyers of ETH (bids) |
--------------------------------------------------------------------------------------
| 3.35718064            | 3195000          | 2322.765         |                      |
| 10.07306967           | 3194000          | 2322.038         |                      |
| 4.1710622             | 3193000          | 2321.311         |                      |
| 22.5008735900000012   | 3192000          | 2320.584         |                      |
| 17.31016963           | 3191000          | 2319.857         |                      |
|                       | 3189000          | 2318.403         | 0.97522874           |
|                       | 3188000          | 2317.676         | 6.58649442           |
|                       | 3187000          | 2316.949         | 28.90471404          |
|                       | 3186000          | 2316.222         | 5.72230368           |
|                       | 3185000          | 2315.495         | 7.89530817           |
--------------------------------------------------------------------------------------
```

#### Prints last 24h traded volume for all exchanges supporting ETH-USDT market

```bash
coincenter volume-day eth-usdt
```

Possible output:

```bash
----------------------------------------------
| Exchange | Last 24h ETH-USDT traded volume |
----------------------------------------------
| binance  | 647760.56051 ETH                |
| huobi    | 375347.704456004546 ETH         |
| kraken   | 1626.32378405 ETH               |
| upbit    | 37.72550547 ETH                 |
----------------------------------------------
```

#### Prints last price of Cardano in Bitcoin for all exchanges supporting it

```bash
coincenter price ada-btc
```

Possible output:

```bash
---------------------------------
| Exchange | ADA-BTC last price |
---------------------------------
| binance  | 0.00003951 BTC     |
| huobi    | 0.00003952 BTC     |
| kraken   | 0.00003955 BTC     |
| upbit    | 0.00003984 BTC     |
---------------------------------
```

#### Prints value of `1000 XML` into SHIB for all exchanges where such conversion is possible

```bash
coincenter conversion 1000xlm-shib
```

Possible output:

```bash
+----------+------------------------------+
| Exchange | 1000 XLM converted into SHIB |
+----------+------------------------------+
| binance  | 6097126.83826878288 SHIB     |
| bithumb  | 6113110.2941176275 SHIB      |
| huobi    | 6083108.03661202044 SHIB     |
| kraken   | 6088227.73071931392 SHIB     |
| kucoin   | 6090943.32410022531 SHIB     |
| upbit    | 6084383.631243834 SHIB       |
+----------+------------------------------+
```