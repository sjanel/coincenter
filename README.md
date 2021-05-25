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

## Balance

Check your balance across supported exchanges at one glance!
```
./coincenter --balance
```
prints a formatted table with sum of assets from all loaded private keys (for all exchanges).
It is also possible to give a list of exchanges (comma separated) to print balance only on those ones.
You can specify an additional currency to which all assets will be converted to have a nice estimation of your total balance with `--balance-cur <curAcronym>`.
For instance, to print total balance on Kraken and Bithumb exchanges, with a summary currency of *Euro*, launch:
```
./coincenter --balance kraken,bithumb --balance-cur eur
```

## Simple Trade

It is possible to realize a simple trade on one exchange by the command line handled automatically by the program, according to different strategies.
Of course, this requires that your private keys for the considered exchange are well settled in the 'config/secret.json' file, and that your balance is adequate. 

Possible strategies:
 - maker: Order placed at limit price (default) 
          Price is continuously adjusted to limit price and will be cancelled at expired time if not fully matched (controlled with `--trade-timeout`)
 - taker: order placed at market price, should be matched immediately
 - adapt: same as maker, except that after `t + timeout - trade-emergency` time (`t` being the start time of the trade) remaining unmatched part is placed at market price to force the trade

Example: "Trade 0.5 BTC to euros on Kraken, in simulated mode (no real order will be placed, useful for tests), with the 'adapt' strategy (maker then taker),
          an emergency mode triggered before 1500 ms of the timeout of 15 seconds."
```
./coincenter --trade 0.5btc-eur,kraken --trade-sim --trade-strategy adapt --trade-emergency 1500 --trade-timeout 15
```

### Trade simulation
Some exchanges (Kraken and Binance for instance) allow to actually query their REST API in simulation mode to validate the query and not perform the trade. It is possible to do this with `coincenter` thanks to `--trade-sim` option. For exchanges which do not support this validation mode, `coincenter` will simply directly finish the trade entirely (taking fees into account) ignoring the trade strategy.

## Check markets order book

Check one or several (one per given exchange) market order books with `--orderbook` option. By default, chosen `depth` is `10`, can be configured with `--orderbook-depth`.

Example: Print ADA (Cardano) - USDT market order book with a depth of 20 on Kraken and Binance:
```
./coincenter --orderbook ada-usdt,kraken,binance --orderbook-depth 20
```

## Withdraw coin

It is possible to withdraw coin with `coincenter` as well, in a synchronized mode (withdraw will check that funds are well received at destination).
Some exchanges require that external addresses are validated prior to their usage in the API (*Kraken* and *Huobi* for instance).

To ensure maximum safety, there are two checks performed by `coincenter` prior to all withdraw launches:
 - External address is not taken as an input parameter, by instead dynamically retrieved from the REST API `getDepositAddress` of the destination exchange
 - Then retrieved deposit address is validated in `config/.depositaddresses.json` which serves as a *postfolio* of trusted addresses

Example: Withdraw 10000 XLM (Stellar) from Bithumb to Huobi:
```
./coincenter --withdraw 10000xlm,bithumb-huobi
```

# Configuration files
Configuration files are all stored in the *config* directory 

## Secrets
*secret.json* holds your private keys. Keep it safe, secret and never commit / push it. It is present in `.gitignore` to avoid mistakes.
`config/secret_test.json` shows the syntax.

## Exchange config
You can exclude currencies in the exchange configuration file (for instance: some unstable fiat currencies in binance).

# Other examples

## Get an overview of your portfolio in Korean Won
```
./coincenter -b --balance-cur krw
```

## Trade 1000 euros to XRP on kraken with a maker strategy
```
./coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-emergency 1500 --trade-timeout 60
```

### Trade 1000 euros to XRP on kraken with a maker strategy in simulation mode
```
./coincenter --trade "1000eur-xrp,kraken" --trade-strategy maker --trade-emergency 1500 --trade-timeout 60 --trade-sim
```

#### Possible output
```
**** Traded 999.99999999954052 EUR into 1221.7681748109101 XRP ****
```

## Prints bithumb and upbit orderbook of Ethereum also converted to euros
```
./coincenter -o eth-krw,bithumb,upbit --orderbook-cur eur
```

### Possible output
```
[2021-05-25 10:22:30.020] [info] Order book of ETH-KRW on bithumb requested with conversion rate 0.000727 EUR
--------------------------------------------------------------------------------------
| Sellers of ETH (asks) | ETH price in KRW | ETH price in EUR | Buyers of ETH (bids) |
--------------------------------------------------------------------------------------
| 7.5279                | 3180000          | 2311.86          |                      |
| 14.5042               | 3179000          | 2311.133         |                      |
| 13.767                | 3178000          | 2310.406         |                      |
| 1.6862                | 3177000          | 2309.679         |                      |
| 9.5308                | 3176000          | 2308.952         |                      |
|                       | 3172000          | 2306.044         | 0.0138               |
|                       | 3171000          | 2305.317         | 6.3197               |
|                       | 3170000          | 2304.59          | 1.616                |
|                       | 3169000          | 2303.863         | 2.4065               |
|                       | 3168000          | 2303.136         | 2.3281               |
--------------------------------------------------------------------------------------
[2021-05-25 10:22:30.020] [info] Order book of ETH-KRW on upbit requested with conversion rate 0.000727 EUR
--------------------------------------------------------------------------------------
| Sellers of ETH (asks) | ETH price in KRW | ETH price in EUR | Buyers of ETH (bids) |
--------------------------------------------------------------------------------------
| 12.834572039999999    | 3201000          | 2327.127         |                      |
| 19.1836957            | 3200000          | 2326.4           |                      |
| 2.01859888            | 3199000          | 2325.673         |                      |
| 0.09736826            | 3198000          | 2324.946         |                      |
| 5.94347545            | 3197000          | 2324.219         |                      |
| 0.83657297            | 3196000          | 2323.492         |                      |
| 0.68518813            | 3194000          | 2322.038         |                      |
| 38.09911129           | 3193000          | 2321.311         |                      |
| 0.2586356             | 3192000          | 2320.584         |                      |
| 3.2692701             | 3190000          | 2319.13          |                      |
| 2.8232                | 3189000          | 2318.403         |                      |
| 2.86476538            | 3188000          | 2317.676         |                      |
| 2.84372958            | 3187000          | 2316.949         |                      |
| 4.26207148            | 3185000          | 2315.495         |                      |
| 0.62962581            | 3183000          | 2314.041         |                      |
|                       | 3180000          | 2311.86          | 0.56930835           |
|                       | 3179000          | 2311.133         | 0.01                 |
|                       | 3178000          | 2310.406         | 2.77698431           |
|                       | 3177000          | 2309.679         | 7.36741461           |
|                       | 3176000          | 2308.952         | 2.58938208           |
|                       | 3175000          | 2308.225         | 13.96513205          |
|                       | 3174000          | 2307.498         | 6.09463114           |
|                       | 3173000          | 2306.771         | 7.126524             |
|                       | 3172000          | 2306.044         | 10.74460863          |
|                       | 3171000          | 2305.317         | 12.21257505          |
|                       | 3170000          | 2304.59          | 35.67871081          |
|                       | 3169000          | 2303.863         | 5.33896981           |
|                       | 3168000          | 2303.136         | 167.72148545         |
|                       | 3167000          | 2302.409         | 7.92094383           |
|                       | 3166000          | 2301.682         | 11.50667905          |
--------------------------------------------------------------------------------------
```
