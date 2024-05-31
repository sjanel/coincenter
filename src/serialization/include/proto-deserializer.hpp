#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "cct_log.hpp"
#include "cct_vector.hpp"
#include "continuous-iterator.hpp"
#include "market-timestamp-set.hpp"
#include "market-timestamp.hpp"
#include "market.hpp"
#include "proto-multiple-messages-handler.hpp"
#include "serialization-tools.hpp"
#include "stringconv.hpp"
#include "time-window.hpp"
#include "timedef.hpp"

namespace cct {

template <class ProtobufObjType, class ProtoToCoincenterObjectsFunc>
class ProtobufObjectsDeserializer {
 public:
  using CoincenterObjectType = std::invoke_result_t<ProtoToCoincenterObjectsFunc, const ProtobufObjType&>;

  explicit ProtobufObjectsDeserializer(std::filesystem::path exchangeSerializedDataPath) noexcept
      : _exchangeSerializedDataPath(std::move(exchangeSerializedDataPath)) {}

  /// Load all markets found on disk which has some data in the given time window
  MarketTimestampSet listMarkets(TimeWindow timeWindow) {
    vector<MarketTimestamp> marketTimestamps;

    std::error_code ec;
    if (std::filesystem::is_directory(_exchangeSerializedDataPath, ec)) {
      for (const auto& marketDirectory : std::filesystem::directory_iterator(_exchangeSerializedDataPath)) {
        auto ts = loadMarket(marketDirectory, timeWindow, ActionType::kCheckPresence).second;
        if (ts != TimePoint{}) {
          const auto& marketPath = marketDirectory.path();

          marketTimestamps.emplace_back(Market{marketPath.filename().string()}, ts);
        }
      }
    } else if (std::filesystem::exists(_exchangeSerializedDataPath)) {
      log::error("{} is not a valid directory: {}", _exchangeSerializedDataPath.string(), ec.message());
    }

    return MarketTimestampSet(std::move(marketTimestamps));
  }

  /// Load all data found on disk for given market for the time window
  vector<CoincenterObjectType> loadMarket(Market market, TimeWindow timeWindow) {
    const std::filesystem::path marketPath = _exchangeSerializedDataPath / std::string_view{market.str()};

    return loadMarket(std::filesystem::directory_entry(marketPath), timeWindow, ActionType::kLoad).first;
  }

 private:
  static bool ValidateTimestamp(const ProtobufObjType& msg, TimeWindow timeWindow) {
    if (!msg.has_unixtimestampinms()) {
      log::error("Invalid data loaded for protobuf object, no unix timestamp set");
      return false;
    }
    return timeWindow.contains(msg.unixtimestampinms());
  }

  enum class ActionType : int8_t { kLoad, kCheckPresence };

  static ContinuousIterator CreateIt(int from, int to, ActionType actionType) {
    if (actionType == ActionType::kCheckPresence) {
      // In check presence mode, we are only interested in the latest timestamp, so we explore towards the past
      std::swap(from, to);
    }
    return {from, to};
  }

  /// Load all data found on disk for given market for the time window
  auto loadMarket(const std::filesystem::directory_entry& marketDirectory, TimeWindow timeWindow,
                  ActionType actionType) {
    std::pair<vector<CoincenterObjectType>, TimePoint> ret;
    if (!marketDirectory.is_directory()) {
      return ret;
    }
    const auto fromDays = std::chrono::floor<std::chrono::days>(timeWindow.from());
    const std::chrono::year_month_day fromYmd{fromDays};
    const std::chrono::hh_mm_ss fromTime{std::chrono::floor<milliseconds>(timeWindow.from() - fromDays)};

    const auto toDays = std::chrono::floor<std::chrono::days>(timeWindow.to());
    const std::chrono::year_month_day toYmd{toDays};
    const std::chrono::hh_mm_ss toTime{std::chrono::floor<milliseconds>(timeWindow.to() - toDays)};

    const auto& marketPath = marketDirectory.path();
    const auto marketFilename = marketPath.filename();
    const Market market(marketFilename.string());

    ProtoToCoincenterObjectsFunc converter(market);

    const int fromYear = static_cast<int>(fromYmd.year());
    const int toYear = static_cast<int>(toYmd.year());

    for (ContinuousIterator yearIt = CreateIt(fromYear, toYear, actionType); yearIt.hasNext();) {
      const auto year = yearIt.next();
      const auto yearPath = marketPath / std::string_view(IntegralToCharVector(year));
      if (!std::filesystem::is_directory(yearPath)) {
        continue;
      }
      const bool isYearFromExtremity = year == fromYear;
      const bool isYearToExtremity = year == toYear;
      const auto fromMonth = isYearFromExtremity ? static_cast<int>(static_cast<unsigned int>(fromYmd.month())) : 1;
      const auto toMonth = isYearToExtremity ? static_cast<int>(static_cast<unsigned int>(toYmd.month())) : 12;

      for (ContinuousIterator monthIt = CreateIt(fromMonth, toMonth, actionType); monthIt.hasNext();) {
        const auto month = monthIt.next();
        const auto monthPath = yearPath / MonthStr(month);
        if (!std::filesystem::is_directory(monthPath)) {
          continue;
        }
        const bool isMonthFromExtremity = isYearFromExtremity && month == fromMonth;
        const bool isMonthToExtremity = isYearToExtremity && month == toMonth;
        const auto fromDay = isMonthFromExtremity ? static_cast<int>(static_cast<unsigned int>(fromYmd.day())) : 1;
        const auto toDay = isMonthToExtremity ? static_cast<int>(static_cast<unsigned int>(toYmd.day())) : 31;

        for (ContinuousIterator dayOfMonthIt = CreateIt(fromDay, toDay, actionType); dayOfMonthIt.hasNext();) {
          const auto dayOfMonth = dayOfMonthIt.next();
          const auto dayPath = monthPath / DayOfMonthStr(dayOfMonth);
          if (!std::filesystem::is_directory(dayPath)) {
            continue;
          }

          const bool isDayFromExtremity = isMonthFromExtremity && dayOfMonth == fromDay;
          const bool isDayToExtremity = isMonthToExtremity && dayOfMonth == toDay;
          const auto fromHour = isDayFromExtremity ? static_cast<int>(fromTime.hours().count()) : 0;
          const auto toHour = isDayToExtremity ? static_cast<int>(toTime.hours().count()) : 23;

          for (ContinuousIterator hourOfDayIt = CreateIt(fromHour, toHour, actionType); hourOfDayIt.hasNext();) {
            const auto hourOfDay = hourOfDayIt.next();
            const auto hourPath = dayPath / ComputeProtoFileName(hourOfDay);
            if (!std::filesystem::exists(hourPath)) {
              continue;
            }

            decltype(std::declval<ProtobufObjType>().unixtimestampinms()) lastTs = 0;

            std::ifstream ifs(hourPath, std::ios::in | std::ios::binary);
            for (ProtobufMessageCompressedReaderIterator protobufMessageReaderIt(ifs);
                 protobufMessageReaderIt.hasNext();) {
              auto msg = protobufMessageReaderIt.next<ProtobufObjType>();
              if (!ValidateTimestamp(msg, timeWindow)) {
                continue;
              }

              // In Check presence mode, we read all the file to retrieve the latest timestamp.
              // There's no other way to do it.
              lastTs = msg.unixtimestampinms();

              if (actionType == ActionType::kLoad) {
                ret.first.push_back(converter(std::move(msg)));
              }
            }

            if (lastTs != 0) {
              if (ret.second == TimePoint{}) {
                ret.second = TimePoint{milliseconds{static_cast<int64_t>(lastTs)}};
              }
              if (actionType == ActionType::kCheckPresence) {
                return ret;
              }
            }
          }
        }
      }
    }
    return ret;
  }

  std::filesystem::path _exchangeSerializedDataPath;
};

}  // namespace cct
