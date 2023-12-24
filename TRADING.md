# Trading

`coincenter` is able to serialize market data and deserialize it later on for future usage.

## Overview

Currently, these two sources of data are serializable into [protobuf](https://protobuf.dev/) objects:

- Market order books
- Public trades

## Configuration

### Compilation

By default, `coincenter` will be built with **protobuf** support (controlled by `cmake` flag `CCT_ENABLE_PROTO` that defaults to `ON`).

It will try to link to a known installation of **protobuf** if found on current system (oldest tested version is `v25`, make sure to use this version at least), otherwise it will download and compile it from sources.

### Serialization configuration

To be able to serialize market data on disk, make sure that you set the **marketDataSerialization** variable to `true` in `exchangeconfig.json` for the exchanges you would like to interact with.
See the [exchange configuration part](CONFIG.md#exchanges-options-description) for more information about how to configure it.

## Serialization of market data

The data will be organized by exchange, then market (asset pair, for instance `BTC-USD`), and finally dates (with directories from **year**, **month**, and finally **day** and files as **hours**).

All will be stored in `coincenter` data directory, under `serialized` sub folder.

Here is an example of the structure of files you will obtain:

```bash
data/serialized/<order-books|trades>/
├── binance
│   ├── BTC-EUR
│   │   └── 2024
│   │       └── 01
│   │           ├── 11
│   │           │   └── 22-00-00_22-59-59.binpb
│   │           ├── 12
│   │           │   ├── 08-00-00_08-59-59.binpb
│   │           │   ├── 09-00-00_09-59-59.binpb
│   │           │   └── 11-00-00_11-59-59.binpb
│   │           └── 14
│   │               ├── 07-00-00_07-59-59.binpb
│   │               └── 08-00-00_08-59-59.binpb
│   ├── ETH-USDT
│   │   └── 2024
│   │       ├── 01
│   │       │   ├── 09
│   │       │   │   ├── 08-00-00_08-59-59.binpb
│   │       │   │   ├── 09-00-00_09-59-59.binpb
│   │       │   │   ├── 10-00-00_10-59-59.binpb
│   │       │   │   ├── 11-00-00_11-59-59.binpb
├── huobi
│   ├── ADA-USDT
│   │   └── 2024
│   │       └── 02
│   │           └── 10
│   │               └── 16-00-00_16-59-59.binpb
│   ├── BTC-EUR
│   │   └── 2024
│   │       └── 01
│   │           ├── 11
│   │           │   └── 22-00-00_22-59-59.binpb
....
```

To retrieve market data, it's possible to either use multi-commands with both `orderbook` and `last-trades` commands stacked together, or you can use the more handy `market-data` option that is basically a combination of the two without the output by default (it has been created only for serialization purposes).

For instance, to retrieve continuously data and serialize them indefinitely, you can use the following command:

```bash
coincenter -r --repeat-time 2s --log warning \
       market-data btc-eur,binance \
       market-data eth-usdt,kucoin \
       market-data ada-usdt,huobi \
       market-data btc-eur,kraken
```

Note the usage of the `-r` (repeat option) to keep querying the data as long as you leave `coincenter` up. It's a good idea to also limit the number of logs with setting console log level to `warning` but not mandatory.

With this command running for an extended period of time, you should obtain a list of files like in the above example.

Stacking `market-data` commands together with different exchanges (like in the above example) will allow `coincenter` to perform the queries in parallel, ensuring optimal frequency of data updates. This optimization may be implemented for other commands in the future, but it's currently supported only for `market-data`.

### Graceful shutdown

Data is flushed on the disk at regular intervals (around 10 minutes). If you wish to restart / shutdown `coincenter` with an infinite `repeat` command to store continuously market data, you can send `SIGINT` or `SIGTERM` so that `coincenter` can gracefully stop after current request and flush its remaining data on disk before shutdown.

## Replaying historic market data

Of course, serialization is useful only if we re-use the data one day. Being **protobuf**, not only `coincenter` could read them, but also other tools, but `coincenter` is also able to read this data.

Locate the `.proto` files in order and feed them to the external tool in case you want to use another program to read the data. Data is stored in hours in streaming mode (see documentation [here](https://protobuf.dev/programming-guides/techniques/#streaming)), and written in compressed format.

If you want to use a third-party tool to read this data, locate the `.proto` files in the `src` directory for your external program to be able to deserialize the `.binpb` files.

We will focus here on `coincenter` features concerning this data.

### Experimental - Testing trading algorithms

`coincenter` embeds a trading simulator engine that is able to be used for any custom trading algorithm that would derive from the interface.

This trading simulator will read chunks of historic data stored in **protobuf** and inject them in trading algorithms.

Locate the `AbstractMarketTrader` class and derive it - you need to return a `TraderCommand` for each market order book and a list of last public trades that occurred at this specific point of time.

TODO: extend this documentation.
