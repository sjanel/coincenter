#pragma once

#include <utility>

#include "apiquerytypeenum.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "duration-schema.hpp"

namespace cct::schema {

using ExchangeQueryUpdateFrequencyConfig =
    FixedCapacityVector<std::pair<QueryType, Duration>, json::reflect<QueryType>::size>;

// Merge src into des update durations  by keeping the minimum duration. May modify src order.
void MergeWith(ExchangeQueryUpdateFrequencyConfig &src, ExchangeQueryUpdateFrequencyConfig &des);

}  // namespace cct::schema