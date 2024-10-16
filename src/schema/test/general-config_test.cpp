#include "general-config.hpp"

#include <gtest/gtest.h>

#include "write-json.hpp"

namespace cct {

TEST(GeneralConfig, WriteMinified) {
  EXPECT_EQ(
      WriteJsonOrThrow(schema::GeneralConfig{}),
      R"({"apiOutputType":"table","fiatConversion":{"rate":"8h"},"log":{"activityTracking":{"commandTypes":["Trade","Buy","Sell","Withdraw","DustSweeper"],"dateFileNameFormat":"%Y-%m","withSimulatedCommands":false},"consoleLevel":"info","fileLevel":"debug","maxFileSize":"5Mi","maxNbFiles":20},"requests":{"concurrency":{"nbMaxParallelRequests":1}},"trading":{"automation":{"deserialization":{"loadChunkDuration":"1w"},"startingContext":{"startBaseAmountEquivalent":"1000 EUR","startQuoteAmountEquivalent":"1000 EUR"}}}})");
}

TEST(GeneralConfig, WriteFormatted) {
  EXPECT_EQ(WriteJsonOrThrow<true>(schema::GeneralConfig{}),
            R"({
  "apiOutputType": "table",
  "fiatConversion": {
    "rate": "8h"
  },
  "log": {
    "activityTracking": {
      "commandTypes": [
        "Trade",
        "Buy",
        "Sell",
        "Withdraw",
        "DustSweeper"
      ],
      "dateFileNameFormat": "%Y-%m",
      "withSimulatedCommands": false
    },
    "consoleLevel": "info",
    "fileLevel": "debug",
    "maxFileSize": "5Mi",
    "maxNbFiles": 20
  },
  "requests": {
    "concurrency": {
      "nbMaxParallelRequests": 1
    }
  },
  "trading": {
    "automation": {
      "deserialization": {
        "loadChunkDuration": "1w"
      },
      "startingContext": {
        "startBaseAmountEquivalent": "1000 EUR",
        "startQuoteAmountEquivalent": "1000 EUR"
      }
    }
  }
})");
}

}  // namespace cct