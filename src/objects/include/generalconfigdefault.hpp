#pragma once

#include "cct_json.hpp"

namespace cct {
struct GeneralConfigDefault {
  static json Prod() {
    // Use a static method instead of an inline static const variable to avoid the infamous 'static initialization order
    // fiasco' problem.

    static const json kProd = R"(
{
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
  }
}
)"_json;
    return kProd;
  }
};
}  // namespace cct