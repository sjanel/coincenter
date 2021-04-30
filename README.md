[![ubuntu](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml)

coincenter
==========

A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface.
Supported exchanges are:
- Kraken
- Bithumb
- Binance
- Upbit (work in progress)
 
 *Table of Contents*
- [coincenter](#coincenter)
    - [Pre-requisites](#pre-requisites)
    - [Install](#install)
      - [As a main project](#as-a-main-project)
      - [As a static library](#as-a-static-library)
      - [Build](#build)
    - [Tests](#tests)
    - [Usage](#usage)
      - [Simple Trade](#simple-trade)
      - [Check markets order book](#check-markets-order-book)
      - [Balance](#balance)
    - [Debug](#debug)
    - [Configuration files](#configuration-files)
      - [Secrets](#secrets)
      - [Exchange config](#exchange-config)
    - [Examples](#examples)
      - [Get an overview of your portfolio in Euros](#get-an-overview-of-your-portfolio-in-euros)
      - [Trade 1000 euros to XRP on kraken with a maker strategy](#trade-1000-euros-to-xrp-on-kraken-with-a-maker-strategy)

### Pre-requisites

You will need to install *OpenSSL* (min version 1.1.0), *cURL*, *cmake* and a C++20 compiler (only *gcc-10* is supported for now) on your system.

For instance, for Unix Debian based systems:
```
sudo apt-get update
sudo apt-get install libcurl4-gnutls-dev libssl-dev cmake g++-10 gcc-10
```

### Install

#### As a main project

**coincenter** can be used as a stand-alone project which provides an executable able to perform most common exchange operations on supported exchanges:
 - Balance
 - Orderbook
 - Trade
 - Withdraw

Simply launch
```
./coincenter --help
```
to see available commands.

#### As a static library

**coincenter** can also be used in a sub project, such as a trading bot for instance. It is the case by default if built as a sub-module in `cmake`.

To build your `cmake` project with **coincenter** library, you can do it with `FetchContent`:
```
include(FetchContent)

FetchContent_Declare(
  coincenter
  GIT_REPOSITORY https://github.com/sjanel/coincenter.git
  GIT_TAG        origin/master
)

FetchContent_MakeAvailable(coincenter)
```
Then, a static library named `coincenter` is defined and you can link it as usual:
```
target_link_libraries(<MyProgram> PRIVATE coincenter)
```
#### Build

This is a C++20 project. Today, it is only partially supported by the main compilers.

Does not (yet) compile with clang (does not support lambdas in unevaluated context), but it has been tested with GCC 10.1 and GCC 10.2.

Other compilers have not been tested yet. If you successfully compiled it on Windows or Mac for instance, please open a PR and update the CI!

`coincenter` uses `cmake`.

Example: to compile it in `Release` mode and `ninja` generator
```
BUILD_MODE=Release; mkdir -p build/${BUILD_MODE} && cd build/${BUILD_MODE} && cmake -GNinja -DCMAKE_BUILD_TYPE=${BUILD_MODE} ../.. && ninja -j 8
```

### Tests

Tests are compiled only if `coincenter` is built as a main project by default. You can set `cmake` flag `CCT_ENABLE_TESTS` to 1 or 0 to change this behavior.

Note that exchanges API are also unit tested. If no private key is found, only public exchanges will be tested, private exchanges will be skipped and unit test will not fail.

### Usage

#### Simple Trade

It is possible to realize a simple trade on one exchange by the command line handled automatically by the program, according to different strategies.
Of course, this requires that your private keys for the considered exchange are well settled in the 'data/secret.json' file, and that your balance is adequate. 

Possible strategies:
 - maker: order placed at limit price (default) 
          price is continuously adjusted to limit price
 - taker: order placed at market price should be matched directly
 - adapt: same as maker, except that order will be updated at market price before the timeout to make it eventually completely matched

Order will be placed at limit price by default, with a maker strategy.

Example: "Trade 0.5 BTC to euros on Kraken, in simulated mode (no real order will be placed, useful for tests), with the 'adapt' strategy (maker then taker),
          an emergency mode triggered before 1500 ms of the timeout of 15 seconds."
```
./coincenter --trade "0.5btc-eur,kraken" --trade-sim --trade-strategy adapt --trade-emergency 1500 --trade-timeout 15
```

#### Check markets order book

Check one or several (one per given exchange) market order books with `--orderbook` option. By default, chosen `depth` is `10`, can be configured with `--orderbook-depth`.
Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance:
```
./coincenter --orderbook ada-usdt,kraken,binance --orderbook-depth 20
```

#### Balance

Check your balance across supported exchanges at one glance! For this, just give `--balance <exchanges>` to print a formatted table with all given exchanges,
separated by commas. Same assets on different exchanges will be summed on the same row.
You can specify an additional currency to which all assets will be converted to have a nice estimation of your total balance with `--balance-cur <curAcronym>`.
For instance, to print all balance on Kraken and Bithumb exchanges, with a summary currency of Euro, launch:
```
./coincenter --balance kraken,bithumb --balance-cur eur
```
Special exchange name `all` allows to print the accumulation of balances from all available private accounts:
```
./coincenter --balance all --balance-cur usdt
```

### Debug

### Configuration files
Configuration files are all stored in the data directory 

#### Secrets
secret.json holds your private keys. Keep it safe (and never commit / push it!). 'data/secret_test.json' shows the syntax.

#### Exchange config
You can exclude currencies in the exchange configuration file (for instance: some unstable fiat currencies in binance).

### Examples

#### Get an overview of your portfolio in Euros
```
./coincenter --balance all --balance-cur eur
```

#### Trade 1000 euros to XRP on kraken with a maker strategy
```
./coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-emergency 1500 --trade-timeout 60
```
