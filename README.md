coincenter
==========

[![docker](https://github.com/sjanel/coincenter/actions/workflows/docker.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/docker.yml)
[![macos](https://github.com/sjanel/coincenter/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/macos.yml)
[![ubuntu](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml)
[![windows](https://github.com/sjanel/coincenter/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/windows.yml)

[![formatted](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml/badge.svg?branch=main)](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/sjanel/coincenter/main/LICENSE)
[![GitHub Releases](https://img.shields.io/github/release/sjanel/coincenter.svg)](https://github.com/sjanel/coincenter/releases)

Command Line Interface (CLI) / library centralizing several crypto currencies exchanges REST API into a single, ergonomic all in one tool with a unified interface.

Main features:

**Market Data**

- Market
- Ticker
- Orderbook
- Traded volume
- Last price
- Last trades

**Private requests**

- Balance
- Trade (buy & sell in several flavors)
- Deposit information (address & tag)
- Recent Deposits
- Recent Withdraws
- Opened orders
- Cancel opened orders
- Withdraw (with check at destination that funds are well received)
- Dust sweeper
  
**Supported exchanges**
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
- [About](#about)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
  - [Input / output](#input--output)
    - [Format](#format)
    - [Logging](#logging)
      - [Activity history](#activity-history)
  - [Public requests](#public-requests)
    - [Health check](#health-check)
    - [Markets](#markets)
      - [Examples](#examples)
    - [Ticker information](#ticker-information)
    - [Order books](#order-books)
    - [Last 24h traded volume](#last-24h-traded-volume)
    - [Last price](#last-price)
    - [Last trades](#last-trades)
    - [Conversion path](#conversion-path)
  - [Private requests](#private-requests)
    - [How to target keys on exchanges](#how-to-target-keys-on-exchanges)
    - [Balance](#balance)
    - [Single Trade](#single-trade)
      - [Options](#options)
      - [Single trade all](#single-trade-all)
      - [Examples with explanation](#examples-with-explanation)
    - [Multi Trade](#multi-trade)
      - [Trade simulation](#trade-simulation)
    - [Buy / Sell](#buy--sell)
      - [Examples with explanation](#examples-with-explanation-1)
    - [Deposit information](#deposit-information)
    - [Recent deposits](#recent-deposits)
      - [Examples](#examples-1)
    - [Recent withdraws](#recent-withdraws)
      - [Examples](#examples-2)
    - [Opened orders](#opened-orders)
    - [Cancel opened orders](#cancel-opened-orders)
    - [Withdraw coin](#withdraw-coin)
      - [Withdraw options](#withdraw-options)
        - [Withdraw refresh time](#withdraw-refresh-time)
        - [Withdraw asynchronous mode](#withdraw-asynchronous-mode)
    - [Dust sweeper](#dust-sweeper)
  - [Monitoring options](#monitoring-options)
    - [Repeat](#repeat)
  - [Examples of use cases](#examples-of-use-cases)
    - [Get an overview of your portfolio in Korean Won](#get-an-overview-of-your-portfolio-in-korean-won)
    - [Trade 1000 euros to XRP on kraken with a maker strategy](#trade-1000-euros-to-xrp-on-kraken-with-a-maker-strategy)
    - [Trade 1000 euros to XRP on kraken with a maker strategy in simulation mode](#trade-1000-euros-to-xrp-on-kraken-with-a-maker-strategy-in-simulation-mode)
    - [Prints conversion paths](#prints-conversion-paths)
    - [Prints all markets trading Stellar (XLM)](#prints-all-markets-trading-stellar-xlm)
    - [Prints bithumb and upbit orderbook of depth 5 of Ethereum and adds a column conversion in euros](#prints-bithumb-and-upbit-orderbook-of-depth-5-of-ethereum-and-adds-a-column-conversion-in-euros)
    - [Prints last 24h traded volume for all exchanges supporting ETH-USDT market](#prints-last-24h-traded-volume-for-all-exchanges-supporting-eth-usdt-market)
    - [Prints last price of Cardano in Bitcoin for all exchanges supporting it](#prints-last-price-of-cardano-in-bitcoin-for-all-exchanges-supporting-it)

</p>
</details>

# About

`coincenter` is not just another random implementation of some exchanges' REST API, it also provides a **higher abstraction** of each platform specificities by providing workarounds for the ones not implementing some queries (for instance, *withdrawal fees* is rarely accessible with the public endpoint, but `coincenter` can handle their query for all exchanges, by using screen scraping techniques of external resources when needed).

Also, it **takes the user by the hand** for more complex queries, like **trades** and **withdraws**, working by default in **synchronized** mode until the operation is finished (*withdraw* will not finish until the funds are **received** and **available** at destination, *trade* will not finish until the opened order is either **matched / canceled**).

Technically speaking, this project is for **C++ enthusiasts** providing an alternative to other crypto exchanges clients often written in higher level languages. It is an illustration of the elegance and possibilities of modern **C++** in an area where it is (sadly) barely chosen.

Of course, all suggestions to improve the project are welcome (should it be bug fixing, support of a new crypto exchange, feature addition / improvements or even technical aspects about the source code and best development practices).

# Installation

See [INSTALL.md](INSTALL.md)

# Configuration

See [CONFIG.md](CONFIG.md)

# Usage

## Input / output

### Format

`coincenter` is a command line tool with ergonomic and easy to remember option schema. You will usually provide this kind of input:

- Command name, with short hand flag or long name (check with `-h` or `--help`)
- Followed by either:
  - Nothing, meaning that command will be applied globally
  - Amount with currency or any currency, separated by dash to specify pairs, or source - destination
  - Comma separated list of exchanges (all are considered if not provided)

Example:

```
                    Ori curr.   exchange1
                        |          |
coincenter --trade 0.05BTC-USDT,kraken,huobi
                    |        |           |
               from amt.  to curr.    exchange2
```

By default, result of command is printed in a formatted table on standard output.
You can also choose a *json* output format with option `-o json`.

### Logging

`coincenter` uses [spdlog](https://github.com/gabime/spdlog) for logging.

`spdlog` is set up asynchronously, and it's possible to log in rotating files in addition of the console, with different levels for each.

By default, it logs in the console with the 'info' level, and in 'debug' in rotating files.

For this, you can configure statically the default level for each in the [config file](CONFIG.md#options-description).
It can be overridden on the command line, with options `--log <log-level>` and `--log-file <log-level>`.

#### Activity history

It is also possible to store relevant commands results (in `data/log/activity_history_YYYY-MM.txt` files) to keep track of the most important commands of `coincenter`.
Each time a command of type present in the user defined list in `log.activityTracking.commandTypes` from `generalconfig.json` file is finished, its result is appended in json format to the corresponding activity history file of the current year and month.

This is useful for instance to keep track of all trades performed and can be later used for analytics of past performance gains / losses.

By default, it stores all types of trades and withdraws results, but the list is configurable to follow your needs.

**Important**: those files contain important information so `coincenter` does not clean them automatically. You can move the old activity history files if they take to much space after a while.

## Public requests

### Health check

`--health-check` pings the exchanges and checks if there are up and running.
It is the first thing that is checked in exchanges unit tests, hence if the health check fails for an exchange, the tests are skipped for the exchange.

### Markets

Use the `--markets` (or `-m`) command to list all markets trading a given currencies. This is useful to check how you can trade your coin.
At least one currency is mandatory, but the list of exchanges is not. If no exchanges are provided, `coincenter` will simply query all supported exchanges and list the markets involving the given currencies if they exist.

One or two (in this case, querying existence of a market) currencies can be given, separated by a `-`.

**Note**: Markets are returned with the currency pair presented in original order from the exchange, as it could give additional information for services relying on this option (even though it's not needed for `--trade` option of `coincenter`)

#### Examples

List all markets involving Ethereum in Huobi

```
coincenter --markets eth,huobi
```

List exchanges where the pair AVAX-USDT is supported

```
coincenter -m avax-usdt
```

### Ticker information

Retrieve ticker information for all markets of one, several or all exchanges with `--ticker [exchange1,exchange2,...]` option.
List of exchanges should be given as lower case, comma separated. But it is optional, all exchanges are considered if not provided.

Example: Print ticker information for kraken and huobi exchanges

```
coincenter --ticker kraken,huobi
```

### Order books

Check market order books of a currency pair with `--orderbook` option. By default, chosen `depth` is `10`, can be configured with `--orderbook-depth`.
Similarly to `--ticker` option, exchanges list is optional. If not provided, all exchanges will be queried and only the market order books from exchange with proposes this market are returned.

In addition, for convenience, you can also specify a currency in which to convert the price if you are more familiar with it. For instance, you want to print the order book of BTC-KRW in Bithumb exchange, but as you are not familiar with Korean won, you want to convert to USD as well. `--orderbook-cur usd` is for this purpose.

Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance

```
coincenter --orderbook ada-usdt,kraken,binance --orderbook-depth 20
```

### Last 24h traded volume

Fast query last 24h traded volume with `--volume-day` option on one market on one, several or all exchanges (as usual, see above options).

Example: Print last 24h traded volume on market XLM-BTC for all exchanges supporting it

```
coincenter --volume-day xlm-btc
```

### Last price

Fast query last traded price with `--price` option on one market on one, several or all exchanges (as usual, see above options).

Example: Print last price on market SOL-BTC for all exchanges supporting it

```
coincenter --price sol-btc
```

### Last trades

Get a sorted list of last trades with `--last-trades` option on one market on one, several or all exchanges.
You can specify the number of last trades to query (for exchanges supporting this option) with `--last-trades-n`.

Example: Print the last 15 trades on DOT-USDT on Binance and Huobi

```
coincenter --last-trades dot-usdt,binance,huobi --last-trades-n 15
```

### Conversion path

From a starting currency to a destination currency, `--conversion` examines the shortest conversion path (in terms of number of conversion) possible to reach the destination currency, on optional list of exchanges.

It will print the result as an ordered list of markets for each exchange.

Option `--conversion` is used internally for [multi trade](#multi-trade) but is provided as a stand-alone query sfor information.

**Note**: when several conversion paths of same length are possible, it will favor the ones not involving fiat currencies.

## Private requests

These requests will require that you have your secret keys in `data/secret/secret.json` file, for each exchange used.
You can check easily that it works correctly with the `--balance` option.

### How to target keys on exchanges

`coincenter` supports several keys per exchange. Here are both ways to target exchange(s) for `coincenter`:

- **`exchange`** matches **all** keys on exchange `exchange`
- **`exchange_keyname`** matches **only** `keyname` on exchange `exchange`
  
All options use this pattern, but they will behave differently if several keys are matched for one exchange. For instance:

- Balance will sum all balance for matching accounts
- Trade and multi trade will share the amount to be trade accross the matching exchanges, depending on the amount available on each (see [Single trade](#single-trade)).

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

| Command                                           | Explanation                                                                                                           |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `coincenter -b kraken`                            | Sum of balances of 'jack' and 'joe' accounts of kraken                                                                |
| `coincenter -b kraken_jack`                       | Only balance of 'jack' account of kraken                                                                              |
| `coincenter -t 1000usdt-sol,kraken`               | `coincenter` will trade a total of 1000 USDT from 'jack' and/or 'joe' (see [Single trade](#single-trade))             |
| `coincenter -t 1000usdt-sol,kraken_joe`           | Perform the trade on 'joe' account only                                                                               |
|                                                   |
| `coincenter --withdraw-apply 1btc,kraken-binance` | <span style="color:red">**Error**</span>: `coincenter` does not know if it should choose 'jack' or 'joe' for withdraw |

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

| Command                                      | Explanation                                              |
| -------------------------------------------- | -------------------------------------------------------- |
| `coincenter -b binance`                      | Only one account in `binance`, this will print `averell` |
| `coincenter -b binance_averell`              | Same as above                                            |
| `coincenter -t 1000usdt-sol,binance`         | **OK** as no ambiguity                                   |
| `coincenter -t 1000usdt-sol,binance_averell` | **OK** as well                                           |

### Balance

Check your available balance across supported exchanges at one glance!

```
coincenter --balance
```

prints a formatted table with sum of assets from all loaded private keys (for all exchanges).
It is also possible to give a list of exchanges (comma separated) to print balance only on those ones.

You can also specify a currency to which all assets will be converted (if possible) to have a nice estimation of your total balance in your currency.
The currency acronym should be at the first position of the comma separated values, the next ones being the accounts.
For instance, to print total balance on Kraken and Bithumb exchanges, with a summary currency of *Euro*:

```
coincenter --balance eur,kraken,bithumb
```

By default, the balance displayed is the available one, with opened orders remaining unmatched amounts deduced.
To include balance in use as well, `--balance-in-use` should be added.

### Single Trade

A single trade per market / exchange can be done in a simple way supporting 3 parameterized price choser strategies.
It is 'single' in the sense that trade is made in one step (see [Multi Trade](#multi-trade)), in an existing market of provided exchange(s) Similarly to other options, list of exchanges is optional. If empty, all exchanges having some balance for given start amount may be considered. Then, exchanges are sorted in decreasing order of available amounts, and are selected until total available reaches the desired one.

Given start amount can be absolute, or relative with the `%` character. In the latter case, the total start amount will be computed from the total available amounts of the matching exchanges. Percentages should not be larger than `100` and may be decimal numbers.

If only one private exchange is given and start amount is absolute, `coincenter` will not query your funds prior to the trade to minimize response time, make sure that inputs are correct or program may throw an exception.

#### Options

**Trade timeout**

A trade is **synchronous** by default with the created order of `coincenter` (it follows the lifetime of the order).
The trade ends when one of the following events occur:

- Order is matched completely
- Order has still some unmatched amount when trade reaches the **timeout**

By default, the trade timeout is set to **30 seconds*. If you wish to change it, you can use `--trade-timeout` option. It expects a time and supports all the common time suffixes `y` (years), `mon` (months), `w` (weeks), `d` (days), `h` (hours), `min` (minutes), `s` (seconds), `ms` (milliseconds) and `us` (microseconds).

Value can contain several units, but do not support decimal values. For instance, use `2min30s` instead of `2.5min`.

Another example: `--trade-timeout 3min30s504ms`

**Trade asynchronous mode**

Above option allows to control the full life time of a trade until it is either fully matched or cancelled (at timeout).
If you want to only **fire and forget** an order placement, you can use `--trade-async` flag.
It will exit the place process as soon as the order is placed, before any query of order information, hence it's the fastest way to place an order.

Note that it's not equivalent of choosing a **zero** trade timeout (above option), here are the differences:

- Order is not cancelled after the trade process in asynchronous mode, whereas it's either cancelled or matched directly in synchronous mode
- Asynchronous trade is faster as it does not even query order information

**Trade Price Strategy**

Possible order price strategies:

- `maker`: order placed at limit price and regularly updated to limit price (default)
- `taker`: order placed at market price, should be matched immediately
- `nibble`: order price continuously set at limit price + (buy)/- (sell) 1. This is a hybrid mode between the above two methods, ensuring continuous partial matches of the order over time, but at a controlled price (market price can be dangerously low for some short periods of time with large sells)

Use command line option `--trade` or `-t` to make a trade from a departure amount.

**Trade Price Picker**

In order to control more precisely the price of the order, `coincenter` supports custom price as well thanks to `--trade-price` option. Note that this is not compatible with above **trade price strategy** option.
`--trade-price` expects either an integer (!= 0) representing a **relative** price compared to the limit price, or a monetary amount representing an **absolute** fixed price (decimal amount with the price currency).
In fact, parser will recognize a relative price if the amount is without any decimals and without any currency. The price currency will be overriden by the engine, but you can add it for readability and to remove ambiguity of an integer price for the parser.

| `--trade-price` value examples | Explanation                                                                                                                       |
| ------------------------------ | --------------------------------------------------------------------------------------------------------------------------------- |
| `35`                           | *relative* price                                                                                                                  |
| `-2`                           | *relative* price                                                                                                                  |
| `34.6`                         | *absolute* price (engine will take the currency from the market of the trade)                                                     |
| `25 XXX`                       | *absolute* price (engine will override the currency from the market of the trade and not consider `XXX`, no error will be raised) |

*Absolute price*

When requesting an absolute price, the order will be placed exactly at this price. Depending on the order book and the limit prices, order may or may not be matched instantly. Double check the price before launching your trade command!
**Notes**:

- such an order will not be compatible with [multi trade](#multi-trade)) because an absolute price makes sense only for a specific market. However, if you ask a multi trade with a fixed price, no error will be raised, and `coincenter` will simply launch a single trade.
- Order price will not be continuously updated over time

*Relative price*

The **relative price** makes it possible to easily settle a price anywhere in the orderbook. The chosen price is **relative** to the order book limit price - it represents how far away the price is related to the 'center' prices. The higher (or lower) the value, the more difficult it will be for the order to be bought - or sold.

A positive value indicates a higher price for a sell, and a lower price for a buy, resulting in a safe position that should not be matched immediately. A negative value is the opposite - which will most probably be matched immediately, depending on the amount of the order.

So, choosing a relative price of `1`: `--trade-price 1` is equivalent as the `maker` trade price strategy, and choosing a relative price of `-1` is equivalent to the `nibble` strategy.

The relative price is, contrary to the **absolute** (fixed) price above, compatible with multi-trade. Also, it is continuously updated over time depending on the `--trade-update-price` option.

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

```
./coincenter -t nB-A --trade-price 3
```

The chosen price will be `0.42`.

#### Single trade all

If you want to simply sell all available amount from a given currency, you can use `--trade-all` option which takes a departure currency instead of an input amount. This command will ask to trade all available amount for all considered exchanges - use with care!

#### Examples with explanation

```
coincenter -t 1btc-usdt
```

Trade 1 BTC to USDT on all exchanges supporting BTC-USDT market and having some available BTC.
| Exchange        | Amount  | Considered amount for trade |
| --------------- | ------- | --------------------------- |
| exchange1_user1 | 0.1 BTC | 0 BTC                       |
| exchange1_user2 | 0.2 BTC | **0.1 BTC**                 |
| exchange2_user1 | 0.3 BTC | **0.3 BTC**                 |
| exchange3_user1 | 0.6 BTC | **0.6 BTC**                 |
| exchange4_user1 | 0 BTC   | 0 BTC                       |

```
coincenter -t 66.6%xlm-btc,kraken,huobi
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

```
coincenter --trade 0.5btc-eur,kraken --trade-sim --trade-strategy nibble --trade-timeout 1min
```

Trade 0.5 BTC to euros on all Kraken accounts, in simulated mode (no real order will be placed, useful for tests), with the 'nibble' strategy, an emergency mode triggered before 2 seconds of the timeout of 1 minute

```
coincenter --trade-all doge-usdt,binance_user1,huobi --trade-strategy taker
```

Trade all DOGE to USDT on Binance account with key name 'user1' and all Huobi accounts, with the 'taker' strategy, keeping default time out and cancel remaining untouched amount when trade expires

You can also choose the behavior of the trade when timeout is reached, thanks to `--trade-timeout-match` option. By default, when the trade expires, the last unmatched order is cancelled. With above option, instead it places a last order at market price (should be matched immediately).

### Multi Trade

If you want to trade coin *AAA* into *CCC* but exchange does not have a *AAA-CCC* market and have *AAA-BBB* and *BBB-CCC*, then it's possible with a multi trade, which will perform a series of trades in order until the destination currency is reached. Other trade options are the same than for a simple trade. `coincenter` starts by evaluating the shortest conversion path to reach *CCC* from *AAA* and then applies the single trades in the correct order to its final goal.

In order to activate multi trades, there are two ways:

- Default behavior is controlled by [multiTradeAllowedByDefault option](CONFIG.md#options-description) in the `exchangeconfig.json` file. If `true`, multi trade is allowed for the given exchange (or all exchanges if under `default` level).
- It can be forced by adding command line flag `--multi-trade`.

In the case it's activated in the config file, `--no-multi-trade` can force deactivation of the multi trade if needed.

#### Trade simulation

Some exchanges (**Kraken** and **Binance** for instance) allow to actually query their REST API in simulation mode to validate the query and not perform the trade. It is possible to do this with `--trade-sim` option. For exchanges which do not support this validation mode, `coincenter` will simply directly finish the trade entirely (taking fees into account) ignoring the trade strategy.

### Buy / Sell

Trade family of commands require that you specify the *start amount* (with the start currency) and the *target currency*. You can use `--buy` and `--sell` options when you have a start amount (for *sell*) or a target amount (for *buy*) only. It's more easy to use, but `coincenter` needs to know which are the [preferred currencies](CONFIG.md#options-description) to which it can *sell* the start amount to, or use as start amount for a *buy*.

Sell option also supports percentage as start amount. In this case, the desired percentage of total available amount of given currency on matching exchanges (the ones specified after the `,` or all if none given as usual) will be sold. To complement this, `--sell-all` option with a start currency instead of an amount is supported as well, which is syntaxic sugar of a sell of `100%` of available amount.

Buy a percentage is not available yet, simply because I am not sure about what should be the behavior of this option. If you have ideas about it, do not hesitate to propose them.

The list of preferred currencies should be filled prior to **buy / sell** command and is statically loaded at start of coincenter. It is an array of currencies ordered by decreasing priority, and they represent the currencies that can be used as target currency for a *sell*, and base currency for a *buy*. See [config](CONFIG.md#options-description) file to see how to set the preferred currencies.

**Behavior**:

- All currencies present in the **preferred currencies** of a given exchange may be used to validate and perform trades. For instance, if you decide to include non stable cryptos (for instance, `BTC` as it is often involves in many pairs), it can be used as a base for a *buy*, or target for a *sell*. Thus as a recommendation, if you do add such currencies as *preferred*, prefer placing them after the fiats and stable coins.
- Several trades may be performed on the same account for a **buy**. For instance: you have both `EUR` and `USDT` and they are both present in the *preferred currencies*, and the desired amount of target currency is high enough such that it would *eat* all of your `EUR` or `USDT`.
- Similarly to **trade** options, `coincenter` tries to minimize the number of actual trades to achieve the desired goal. So it will favor exchanges and currencies with the largest available amount.
- For the `buy`, trade fees are taken into account to attempt to reach exactly the desired **net** amount.

#### Examples with explanation

```bash
coincenter --sell 1 BTC,exchange1,exchange3_user1
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
coincenter --buy 2.5ETH
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

A total of `2 ETH` will be bought, from **600 USDT** on `exchange1_user2`, **800 EUR** and **240 USDT** from `exchange3_user1` and **500000 KRW** from `exchange4_user1`. As you can see in this example, it's possible to make several trades per account for one `--buy` request.

### Deposit information

You can retrieve one deposit address and tag from your accounts on a given currency provided that the exchange can receive it with `--deposit-info` option. It requires a currency code, and an optional list of exchanges (private or public names, see [Handle several accounts per exchange](#handle-several-accounts-per-exchange)). If no exchanges are given, program will query all accounts and retrieve the possible ones.

If for a given exchange account a deposit address is not created yet, `coincenter` will create one automatically.

Only one deposit address per currency / account is returned.

**Important note**: only addresses which are validated against the `depositaddresses.json` file will be returned (unless option `validateDepositAddressesInFile` is set to `false` in `static/exchangeconfig.json` file). This file allows you to control addresses to which `coincenter` can send funds, even if you have more deposit addresses already configured.

### Recent deposits

To retrieve the most recent deposits, use the `--deposits` option. Currency is optional - if not provided, deposits in all currencies will be returned.

Some exchanges do not provide a method to retrieve recent deposits for more than one currency (Kraken and Bithumb for instance).
If you request all currencies on these exchanges, `coincenter` will not query deposits for all existing currencies, but only for the currencies present in your balance to decrease the number of queries. Still, it's an heuristic so it may of course miss other deposits.

By default, it returns all deposits on given currency, or any currency. You can filter the deposits with options:

- `--deposits-max-age <time>` to set a maximum age for the deposits to retrieve
- `--deposits-min-age <time>` to set a minimum age for the deposits to retrieve
- `--deposits-id <id1,id2,...>` to retrieve only deposits with given IDs

#### Examples

`coincenter --deposits kraken --deposits-max-age 2w`: retrieves all deposits of the last two weeks

`coincenter --deposits eth --deposits-id myid1,myid2`: retrieves all deposits of Ethereum, if they have either id `myid1` or `myid2`

### Recent withdraws

Similarly to deposits, you can query the recent **withdraws** with command `--withdraws`. It works the same ways as deposits - you just need to replace `deposits` with `withdraws` in the option name (for instance, `--withdraws-max-age` specifies the maximum age for the withdraws to retrieve).

#### Examples

Retrieve all recent withdraws from all accounts:
```
coincenter --withdraws
```

Retrieve the withdraw of XRP on Upbit with id `myid3`

```
coincenter --withdraws upbit,xrp --withdraws-id myid3
```

### Opened orders

Use option `--orders-opened` to retrieve and print currently opened orders on your accounts. You can provide only one currency code, two currency codes separated with a `-`, and / or a list of exchanges on which to filter the orders. It is possible to not specify anything as well, in this case, all opened orders will be returned.

Orders can be filtered according to placed time with options `--orders-min-age` and `--orders-max-age` specifying respectively the minimum and maximum age of the orders.

Finally, they can also be filtered by their ID, thanks to `--orders-id` option. Give a list of comma separated string Ids.

Examples:

```bash
coincenter --orders-opened --orders-min-age 10min            # Retrieve all opened orders placed within the last 10 minutes
coincenter --orders-opened kraken                            # Retrieve all opened orders on Kraken only
coincenter --orders-opened eth --orders-max-age 1w           # Retrieve all opened orders of at most 1 week old, on markets involving Ethereum on all exchanges
coincenter --orders-opened xlm-usdt                          # Retrieve all opened orders matching markets USDT-XLM or XLM-USDT
coincenter --orders-opened btc-krw,upbit,bithumb             # Retrieve all opened orders on BTC-KRW / KRW-BTC on Bithumb and Upbit
coincenter --orders-opened binance_joe --orders-id OID1,OID2 # Check if 'OID1' and 'OID2' are still opened on 'joe' account on Binance
```

### Cancel opened orders

Opened orders can be cancelled with `--orders-cancel` option, with the same filter arguments as opened orders option above.
**Beware**, no filtering options matches all opened orders, so this command:

```bash
coincenter --orders-cancel
```

will cancel all opened orders on all available exchanges. You can first check the current open orders and order cancellations by ID:

```bash
coincenter --orders-cancel --orders-id OID1
```

cancels order Id OID1 only, on the exchange where it is found on (no error is raised if no such opened order has this ID).

### Withdraw coin

It is possible to withdraw crypto currency with `coincenter` as well, in either a **synchronized** mode (withdraw will check that funds are well received at destination) or **asynchronous** mode. Either an absolute amount can be specified, or a percentage (`10xrp` or `25%xrp` for instance). `--withdraw-apply-all` is a convenient command wrapper of `--withdraw-apply 100%`.

Some exchanges require that external addresses are validated prior to their usage in the API (*Kraken* and *Huobi* for instance).

To ensure maximum safety, there are two checks performed by `coincenter` prior to all withdraw launches:

- External address is not taken as an input parameter, but instead dynamically retrieved from the REST API `getDepositAddress` of the destination exchange (see [Deposit information](#deposit-information))
- By default (can be configured in `static/exchangeconfig.json` with boolean value `validateDepositAddressesInFile`) deposit address is validated in `<DataDir>/secret/depositaddresses.json` which serves as a *portfolio* of trusted addresses

Example: Withdraw 10000 XLM (Stellar) from Bithumb to Huobi:

```
coincenter --withdraw-apply 10000xlm,bithumb-huobi
```

#### Withdraw options

##### Withdraw refresh time

You can customize the withdraw refresh time of the periodic checks during the continuous checking of withdraw status and receive at destination in **synchronous** mode.
For that, specify a different time period with `--withdraw-refresh-time` that expects a string representation of a time.

Defaults to 5 seconds.

##### Withdraw asynchronous mode

By default `coincenter` will exit withdraw process only once destination has received the funds.
You can change the behavior to an **asynchronous** mode thanks to `--withdraw-async` option, which is like a fire and forget mode. Once the withdraw is initiated, withdraw process is finished and the withdraw is not followed anymore.

### Dust sweeper

If you are annoyed with this kind of balance:

```
| BTG      | 0.0000442                |
| DASH     | 0.00002264               |
| DOT      | 0.00009634               |
```

and you would like to "clean" these small amounts to zero, then *dust sweeper* is the option to use!

When user defines a threshold amount on a given currency, `--dust-sweeper` can be used to attempt to sell all amount (if below this threshold) trying to reach exactly 0 amount on this currency for a clean balance.
The algorithm used by `coincenter` is a simple heuristic:

- it finds the best markets involving currency to be sold
- it tries to sell all immediately.
- if full selling is not possible, it buys a small amount of this currency
- retry until either maximum number of steps (configurable) is reached or balance is exactly 0 on given currency

Example: Attempts to clean amount of BTG on bithumb and upbit

```
coincenter --dust-sweeper btg,bithumb,upbit
```

## Monitoring options

`coincenter` can export metrics to an external instance of `Prometheus` thanks to its implementation of [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) client. Refer to [Build with monitoring support](#build-with-monitoring-support) section to know how to build `coincenter` with it.

Currently, its support is experimental and in development for all major options of `coincenter` (private and market data requests).
The metrics are exported in *push* mode to the gateway at each new query. You can configure the IP address, port, username and password (if any) thanks to command line options (refer to the help to see their names).

### Repeat

`coincenter` commands are normally executed only once, and program is terminated after it.
To continuously query the same option to export regular metrics to Prometheus, you can use `--repeat` option. **Warning**: for trades and withdraw commands, use with care.

Without a following numeric value, the command will repeat endlessly. You can fix a specific number of repeats by giving a number.

Between each repeat you can set a waiting time with `--repeat-time` option which expects a time duration.

## Examples of use cases

### Get an overview of your portfolio in Korean Won

```
coincenter -b krw
```

### Trade 1000 euros to XRP on kraken with a maker strategy

```
coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-timeout 60s
```

### Trade 1000 euros to XRP on kraken with a maker strategy in simulation mode

```
coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-timeout 1min --trade-sim
```

Possible output

```
**** Traded 999.99999999954052 EUR into 1221.7681748109101 XRP ****
```

### Prints conversion paths

How to change Yearn Finance to Bitcoin on all exchanges

```
coincenter -c yfi-btc
```

Possible output

```
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

### Prints all markets trading Stellar (XLM)

```
coincenter -m xlm
```

Possible output

```
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

### Prints bithumb and upbit orderbook of depth 5 of Ethereum and adds a column conversion in euros

```
coincenter -o eth-krw,bithumb,upbit --orderbook-cur eur --orderbook-depth 5
```

Possible output

```
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

### Prints last 24h traded volume for all exchanges supporting ETH-USDT market

```
coincenter --volume-day eth-usdt
```

Possible output:

```
----------------------------------------------
| Exchange | Last 24h ETH-USDT traded volume |
----------------------------------------------
| binance  | 647760.56051 ETH                |
| huobi    | 375347.704456004546 ETH         |
| kraken   | 1626.32378405 ETH               |
| upbit    | 37.72550547 ETH                 |
----------------------------------------------
```

### Prints last price of Cardano in Bitcoin for all exchanges supporting it

```
coincenter --price ada-btc
```

Possible output:

```
---------------------------------
| Exchange | ADA-BTC last price |
---------------------------------
| binance  | 0.00003951 BTC     |
| huobi    | 0.00003952 BTC     |
| kraken   | 0.00003955 BTC     |
| upbit    | 0.00003984 BTC     |
---------------------------------
```
