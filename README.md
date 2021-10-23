[![alpine_docker](https://github.com/sjanel/coincenter/actions/workflows/alpine_docker.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/alpine_docker.yml)
[![macos](https://github.com/sjanel/coincenter/actions/workflows/macos.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/macos.yml)
[![ubuntu](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/ubuntu.yml)
[![windows](https://github.com/sjanel/coincenter/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/windows.yml)

[![formatted](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml/badge.svg?branch=master)](https://github.com/sjanel/coincenter/actions/workflows/clang-format-check.yml)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/sjanel/coincenter/master/LICENSE)
[![GitHub Releases](https://img.shields.io/github/release/sjanel/coincenter.svg)](https://github.com/sjanel/coincenter/releases)

coincenter
==========

A C++ Command Line Interface (CLI) / library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface.

Main features:

 - Market
 - Orderbook
 - Traded volume
 - Last Price
 - Balance
 - Trade (in several flavors)
 - Withdraw (with check at destination that funds are well received)
  
Supported exchanges are:
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
  - [Public docker image](#public-docker-image)
  - [Prerequisites](#prerequisites)
    - [Linux](#linux)
      - [Debian / Ubuntu](#debian--ubuntu)
      - [Alpine](#alpine)
    - [Windows](#windows)
  - [Build](#build)
    - [With cmake](#with-cmake)
      - [cmake build options](#cmake-build-options)
      - [As a static library](#as-a-static-library)
    - [With Docker](#with-docker)
      - [Build](#build-1)
      - [Run](#run)
- [Configuration](#configuration)
  - [Important files](#important-files)
    - [secret/secret.json](#secretsecretjson)
      - [Handle several accounts per exchange](#handle-several-accounts-per-exchange)
    - [static/exchangeconfig.json](#staticexchangeconfigjson)
- [Tests](#tests)
- [Usage](#usage)
  - [Balance](#balance)
  - [Simple Trade](#simple-trade)
  - [Multi Trade](#multi-trade)
    - [Trade simulation](#trade-simulation)
  - [Check markets order book](#check-markets-order-book)
  - [Withdraw coin](#withdraw-coin)
  - [Other examples](#other-examples)
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

This project is for **C++** and **crypto enthusiasts** providing an alternative to other crypto exchanges clients often written in higher level languages. 
At the beginning, it started as a experimental project aiming to learn modern C++ (**C++17** and **C++20**), **cmake** and practice all aspects of large project development such as CI/CD, building and documentation.
All suggestions to improve the project are welcome (should it be bug fixing, support of a new crypto exchange, feature addition / improvements or even technical aspects about the source code and best development practices).

# Installation

## Public docker image

If you don't want to build `coincenter` locally, you can just download the public docker image, corresponding to the latest version of branch `main`.

```
docker run -t sjanel/coincenter -h
```

Docker image does not contain additional `data` directory needed by `coincenter` (see ([Configuration](#configuration)))

To bind your 'data' directory from host to the docker container, you can use `--mount` option:

```
docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
```

## Prerequisites

- **Git**
- **C++** compiler supporting C++20 (gcc >= 10, clang >= 13, MSVC >= 19.28).
- **CMake** >= 3.15
- **curl** >= 7.58.0
- **openssl** >= 1.1.0

### Linux

#### Debian / Ubuntu

```
sudo apt update && sudo apt install libcurl4-gnutls-dev libssl-dev cmake g++-10
```

#### Alpine

With `ninja` generator for instance:
```
sudo apk update && sudo apk upgrade && sudo apk add g++ libc-dev curl-dev cmake ninja git linux-headers
```

You can refer to the provided `Dockerfile` for more information.

### Windows

On Windows, the easiest method is to use [chocolatey](https://chocolatey.org/install) to install **curl** and **OpenSSL**:

```
choco install curl openssl
```

Then, locate where curl is installed (by default, should be in `C:\ProgramData\chocolatey\lib\curl\tools\curl-xxx`, let's note this `CURL_DIR`) and add both `CURL_DIR/lib` and `CURL_DIR/bin` in your `PATH`. From this step, **curl** and **OpenSSL** can be found by `cmake` and will be linked statically to the executables.

## Build

### With cmake

This is a **C++20** project. Today, it is only partially supported by the main compilers.

Tested compilers:
 - GCC version >= 10
 - Clang version >= 13
 - MSVC version >= 19.28

Other compilers have not been tested yet.

#### cmake build options

| Option             | Default              | Description                                     |
| ------------------ | -------------------- | ----------------------------------------------- |
| `CCT_ENABLE_TESTS` | `ON` if main project | Build and launch unit tests                     |
| `CCT_BUILD_EXEC`   | `ON` if main project | Build an executable instead of a static library |
| `CCT_ENABLE_ASAN`  | `ON` if Debug mode   | Compile with AddressSanitizer                   |

Example on Linux: to compile it in `Release` mode and `ninja` generator
```
mkdir -p build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && ninja
```

On Windows, you can use your preferred IDE to build `coincenter` (**Visual Studio Code**, **Visual Studio 2019**, etc), or build it from command line, with generator `-G "Visual Studio 16 2019"`. Refer to the GitHub Windows workflow to have the detailed installation steps.

#### As a static library

**coincenter** can also be used as a sub project, such as a trading bot for instance. It is the case by default if built as a sub-module in `cmake`.

To build your `cmake` project with **coincenter** library, you can do it with `FetchContent`:
```
include(FetchContent)

FetchContent_Declare(
  coincenter
  GIT_REPOSITORY https://github.com/sjanel/coincenter.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(coincenter)
```
Then, a static library named `coincenter` is defined and you can link it as usual:
```
target_link_libraries(<MyProgram> PRIVATE coincenter)
```

### With Docker

A **Docker** image is hosted in the public **Docker hub** registry with the name *sjanel/coincenter*, corresponding to latest successful build of `main` branch by the CI.

You can create your own **Docker** image of `coincenter`. It uses **Alpine** Linux distribution as base and multi stage build to reduce the image size.
Build options (all optional):

CMake build mode
`BUILD_MODE` (default: Release)

Compile and launch tests
`TEST` (default: 0)

Activate Address Sanitizer
`ASAN` (default: 0)

#### Build

```
docker build --build-arg BUILD_MODE=Release -t local-coincenter .
```

#### Run

```
docker run -ti -e "TERM=xterm-256color" local-coincenter --help
```

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

#### Handle several accounts per exchange

`coincenter` supports several keys per exchange. In this case, `coincenter` will need additional information for some queries (`trade` and `withdraw` for instance) to select the desired exchange account for the command. Some queries, such as `balance`, work without specifying the account, but the behavior is different: all accounts will be aggregated (balance will be summed for the `balance` query). 

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

| Command                                 | Explanation                                                                                              |
| --------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `coincenter -b kraken`                  | Sum of balances of 'jack' and 'joe' accounts of kraken                                                   |
| `coincenter -b kraken_jack`             | Only balance of 'jack' account of kraken                                                                 |
| `coincenter -t 1000usdt-sol,kraken`     | <span style="color:red">**Error**</span>: `coincenter` does not know if it should choose 'jack' or 'joe' |
| `coincenter -t 1000usdt-sol,kraken_joe` | **OK**: perform the trade on 'joe' account                                                               |

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
| `coincenter -t 1000usdt-sol,binance`         | **OK**, no ambiguity                                     |
| `coincenter -t 1000usdt-sol,binance_averell` | **OK** as well                                           |

### static/exchangeconfig.json

This json file should follow this specific format:
```yaml
  - top level option:
    - default:
      - some option: default value
      - another option: default value
    - exchange:
      - some exchange:
        - some option: override value
        - another option: default value
      - another exchange:
        - some option: override value
```

Currently, options are set from two ways:
- **Comma separated values** are aggregated for each exchange with the 'default' values (if present)
- **Single values** are retrieved in a 'bottom first' priority model, meaning that if a value is specified for an exchange name, it is chosen. Otherwise, it checks at the default value for this option, and if again not present, uses a hardcoded default one (cf in the code).

As an example, consider this file:
```json
{
  "asset": {
    "default": {
      "withdrawexclude": "BTC"
    },
    "exchange": {
      "binance": {
        "withdrawexclude": "BQX"
      },
      "kraken": {
        "withdrawexclude": "EUR,KFEE"
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

| Exchange | `asset/withdrawexclude` | `tradefees/maker` |
| -------- | ----------------------- | ----------------- |
| Binance  | `BTC,BQX`               | `0.1`             |
| Kraken   | `BTC,EUR,KFEE`          | `0.1`             |
| Bithumb  | `BTC`                   | `0.25`            |

Refer to the hardcoded default json example as a model in case of doubt.

# Tests

Tests are compiled only if `coincenter` is built as a main project by default. You can set `cmake` flag `CCT_ENABLE_TESTS` to 1 or 0 to change this behavior.

Note that exchanges API are also unit tested. If no private key is found, only public exchanges will be tested, private exchanges will be skipped and unit test will not fail.

# Usage

## Balance

Check your balance across supported exchanges at one glance!
```
coincenter --balance
```
prints a formatted table with sum of assets from all loaded private keys (for all exchanges).
It is also possible to give a list of exchanges (comma separated) to print balance only on those ones.
You can specify an additional currency to which all assets will be converted to have a nice estimation of your total balance with `--balance-cur <curAcronym>`.
For instance, to print total balance on Kraken and Bithumb exchanges, with a summary currency of *Euro*, launch:
```
coincenter --balance kraken,bithumb --balance-cur eur
```

## Simple Trade

A simple trade per market / exchange can be done in a user friendly way supporting 3 parameterized strategies.
It is 'Simple' in the sense that trade is made in one step (see ([Multi Trade](#multi-trade))), in an existing market of provided exchange.
Of course, this requires that your private keys for the considered exchange are well settled in the `<DataDir>/secret/secret.json` file, and that your balance is sufficient. When unnecessary, `coincenter` will not query your funds prior to the trade to minimize response time, make sure that inputs are correct or program may throw an exception.

Possible strategies:
 - `maker`: Order placed at limit price (default) 
          Price is continuously adjusted to limit price and will be cancelled at expired time if not fully matched (controlled with `--trade-timeout`)
 - `taker`: order placed at market price, should be matched immediately
 - `adapt`: same as maker, except that after `t + timeout - trade-emergency` time (`t` being the start time of the trade) remaining unmatched part is placed at market price to force the trade

Example: "Trade 0.5 BTC to euros on Kraken, in simulated mode (no real order will be placed, useful for tests), with the 'adapt' strategy (maker then taker),
          an emergency mode triggered before 2 seconds of the timeout of 15 seconds."
```
coincenter --trade 0.5btc-eur,kraken --trade-sim --trade-strategy adapt --trade-emergency 2s --trade-timeout 15s
```

## Multi Trade

If you want to trade coin *AAA* into *CCC* but exchange does not have a *AAA-CCC* market and have *AAA-BBB* and *BBB-CCC*, then it's possible with a multi trade by changing `--trade` into `--multitrade`. Options are the same than for a simple trade. `coincenter` starts by evaluating the shortest conversion path to reach *CCC* from *AAA* and then applies the single trades in the correct order to its final goal.

### Trade simulation

Some exchanges (**Kraken** and **Binance** for instance) allow to actually query their REST API in simulation mode to validate the query and not perform the trade. It is possible to do this with `--trade-sim` option. For exchanges which do not support this validation mode, `coincenter` will simply directly finish the trade entirely (taking fees into account) ignoring the trade strategy.

## Check markets order book

Check one or several (one per given exchange) market order books with `--orderbook` option. By default, chosen `depth` is `10`, can be configured with `--orderbook-depth`.

Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance:
```
coincenter --orderbook ada-usdt,kraken,binance --orderbook-depth 20
```

## Withdraw coin

It is possible to withdraw coin with `coincenter` as well, in a synchronized mode (withdraw will check that funds are well received at destination).
Some exchanges require that external addresses are validated prior to their usage in the API (*Kraken* and *Huobi* for instance).

To ensure maximum safety, there are two checks performed by `coincenter` prior to all withdraw launches:
 - External address is not taken as an input parameter, but instead dynamically retrieved from the REST API `getDepositAddress` of the destination exchange
 - By default (can be configured in `static/exchangeconfig.json`) deposit address is validated in `<DataDir>/secret/depositaddresses.json` which serves as a *portfolio* of trusted addresses

Example: Withdraw 10000 XLM (Stellar) from Bithumb to Huobi:
```
coincenter --withdraw 10000xlm,bithumb-huobi
```

## Other examples

### Get an overview of your portfolio in Korean Won
```
coincenter -b --balance-cur krw
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
--------------------------------------
| Exchange | Fastest conversion path |
--------------------------------------
| binance  | YFI-BTC                 |
| bithumb  | YFI-KRW-BTC             |
| huobi    | YFI-BTC                 |
| kraken   | YFI-BTC                 |
| upbit    | --- Impossible ---      |
--------------------------------------
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