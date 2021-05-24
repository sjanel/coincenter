[![ubuntu](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml)
[![alpine_docker](https://github.com/sjanel/coincenter/actions/workflows/alpine_docker.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/alpine_docker.yml)
[![windows](https://github.com/sjanel/coincenter/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/windows.yml)

[![formatted](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml)

coincenter
==========

A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface.

Supported exchanges are:
| Exchange | Link                                                                            |
| -------- | :-----------------------------------------------------------------------------: |
| Binance  | [<img src="./resources/binancelogo.svg" width="170">](https://www.binance.com/) |
| Bithumb  | [<img src="./resources/bithumblogo.svg" width="55">](https://www.bithumb.com/)  |
| Huobi    | [<img src="./resources/huobilogo.svg" width="160">](https://www.huobi.com/)     |
| Kraken   | [<img src="./resources/krakenlogo.svg" width="90">](https://www.kraken.com/)    |
| Upbit    | [<img src="./resources/upbitlogo.svg" width="135">](https://www.upbit.com/)     |
 
 *Table of Contents*
- [coincenter](#coincenter)
- [Install](#install)
  - [Pre-requisites](#pre-requisites)
    - [Linux](#linux)
    - [Windows](#windows)
  - [As an executable (CLI tool)](#as-an-executable-cli-tool)
  - [As a static library](#as-a-static-library)
  - [Build](#build)
    - [From source](#from-source)
    - [From Docker](#from-docker)
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

# Install

## Pre-requisites

You will need to install *OpenSSL* (min version 1.1.0), *cURL*, *cmake* and a *C++20* compiler on your system.

### Linux

For instance, for Unix Debian based systems (tested on Ubuntu 18 & 20):
```
sudo apt-get update
sudo apt-get install libcurl4-gnutls-dev libssl-dev cmake g++-10 gcc-10
```

### Windows

On Windows, the easiest method is to use [chocolatey](https://chocolatey.org/install) to install *cURL* and *OpenSSL*:

```
choco install curl openssl
```

Then, locate where curl is installed (by default, should be in `C:\ProgramData\chocolatey\lib\curl\tools\curl-xxx`, let's note this `CURL_DIR`) and add both `CURL_DIR/lib` and `CURL_DIR/bin` in your `PATH`. From this step, **cURL** and **OpenSSL** can be found by `cmake` and will be linked statically to the executables.

## As an executable (CLI tool)

**coincenter** can be used as a stand-alone project which provides an executable able to perform most common exchange operations on supported exchanges:
 - Balance
 - Orderbook
 - Trade
 - Withdraw

Simply launch the help command for more information
```
./coincenter --help
```
**Warning :** you will need to install your API keys for some commands to work ([Configuration files](#configuration-files))

## As a static library

**coincenter** can also be used as a sub project, such as a trading bot for instance. It is the case by default if built as a sub-module in `cmake`.

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

## Build

### From source

This is a C++20 project. Today, it is only partially supported by the main compilers.

Tested compilers:
 - GCC version >= 10
 - MSVC version >= 19.28

As of May 2021, latest *clang* compiler does not support lambdas in unevaluated context yet and thus cannot compile `coincenter`.

Other compilers have not been tested yet.

`coincenter` uses `cmake`.

Example on Linux: to compile it in `Release` mode and `ninja` generator
```
BUILD_MODE=Release; mkdir -p build/${BUILD_MODE} && cd build/${BUILD_MODE} && cmake -GNinja -DCMAKE_BUILD_TYPE=${BUILD_MODE} ../.. && ninja -j 8
```

On Windows, you can use your preferred IDE to build `coincenter` (**Visual Studio Code**, **Visual Studio 2019**, etc), or build it from command line, with generator `-G "Visual Studio 16 2019"`. Refer to the GitHub Windows workflow to have the detailed installation steps.

### From Docker

Build
```
docker build --build-arg test=1 --build-arg mode=Release -t coincenter .
```
Run
```
docker run -ti -e "TERM=xterm-256color" coincenter:latest --help
```

To keep secrets, build using the keepsecrets option. **Warning : image will contain your secrets.**
```
docker build --build-arg keepsecrets=1 -t coincenter .
```


# Tests

Tests are compiled only if `coincenter` is built as a main project by default. You can set `cmake` flag `CCT_ENABLE_TESTS` to 1 or 0 to change this behavior.

Note that exchanges API are also unit tested. If no private key is found, only public exchanges will be tested, private exchanges will be skipped and unit test will not fail.

# Usage

## Simple Trade

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

## Check markets order book

Check one or several (one per given exchange) market order books with `--orderbook` option. By default, chosen `depth` is `10`, can be configured with `--orderbook-depth`.
Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance:
```
./coincenter --orderbook ada-usdt,kraken,binance --orderbook-depth 20
```

## Balance

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

# Configuration files
Configuration files are all stored in the data directory 

## Secrets
secret.json holds your private keys. Keep it safe (and never commit / push it!). 'data/secret_test.json' shows the syntax.

## Exchange config
You can exclude currencies in the exchange configuration file (for instance: some unstable fiat currencies in binance).

# Examples

## Get an overview of your portfolio in Euros
```
./coincenter --balance all --balance-cur eur
```

## Trade 1000 euros to XRP on kraken with a maker strategy
```
./coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-emergency 1500 --trade-timeout 60
```
