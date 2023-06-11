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
  "log": {
    "activityTracking": {
      "commandTypes": [
        "Trade",
        "Buy",
        "Sell",
        "Withdraw",
        "DustSweeper"
      ],
      "dateFileNameFormat": "%Y-%m"
    },
    "consoleLevel": "info",
    "fileLevel": "debug",
    "maxNbFiles": 20,
    "maxFileSize": "5Mi"
  },
  "fiatConversion": {
    "rate": "8h"
  }
}
)"_json;
    return kProd;
  }
};
}  // namespace cct