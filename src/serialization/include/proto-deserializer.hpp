#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>

#include "cct_format.hpp"
#include "cct_log.hpp"
#include "cct_vector.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "proto-multiple-messages-handler.hpp"
#include "serialization-tools.hpp"
#include "time-window.hpp"
#include "timedef.hpp"

namespace cct {

template <class ProtobufObjType, class ProtoToCoincenterObjectsFunc>
class ProtobufObjectsDeserializer {
 public:
  using CoincenterObjectType = std::invoke_result_t<ProtoToCoincenterObjectsFunc, const ProtobufObjType&>;

  explicit ProtobufObjectsDeserializer(std::string exchangeSerializedDataPath) noexcept
      : _exchangeSerializedDataPath(std::move(exchangeSerializedDataPath)) {}

  /// Load all markets found on disk which has some data in the given time window
  MarketVector listMarkets(TimeWindow timeWindow) {
    MarketVector ret;

    if (!std::filesystem::is_directory(_exchangeSerializedDataPath)) {
      return ret;
    }

    for (const auto& marketDirectory : std::filesystem::directory_iterator(_exchangeSerializedDataPath)) {
      if (loadMarket(marketDirectory, timeWindow, ActionType::kCheckPresence).second) {
        const auto& marketPath = marketDirectory.path();
        const auto marketStr = marketPath.filename().string();

        ret.emplace_back(marketStr);
      }
    }

    std::ranges::sort(ret);

    return ret;
  }

  /// Load all data found on disk for given market for the time window
  vector<CoincenterObjectType> loadMarket(Market market, TimeWindow timeWindow) {
    std::string marketPathStr(_exchangeSerializedDataPath);
    marketPathStr.push_back('/');
    marketPathStr.append(market.str());

    std::filesystem::path marketPath(marketPathStr);

    return loadMarket(std::filesystem::directory_entry(marketPath), timeWindow);
  }

  /// Load all data found on disk for given market for the time window
  vector<CoincenterObjectType> loadMarket(const std::filesystem::directory_entry& marketDirectory,
                                          TimeWindow timeWindow) {
    return loadMarket(marketDirectory, timeWindow, ActionType::kLoad).first;
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

  /// Load all data found on disk for given market for the time window
  std::pair<vector<CoincenterObjectType>, bool> loadMarket(const std::filesystem::directory_entry& marketDirectory,
                                                           TimeWindow timeWindow, ActionType actionType) {
    std::pair<vector<CoincenterObjectType>, bool> ret;
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

    for (auto year = fromYear; year <= toYear; ++year) {
      const auto yearPath = marketPath / format("{:04}", year);
      if (!std::filesystem::is_directory(yearPath)) {
        continue;
      }
      const bool isYearFromExtremity = year == fromYear;
      const bool isYearToExtremity = year == toYear;
      const auto fromMonth = isYearFromExtremity ? static_cast<unsigned int>(fromYmd.month()) : 1U;
      const auto toMonth = isYearToExtremity ? static_cast<unsigned int>(toYmd.month()) : 12U;
      for (auto month = fromMonth; month <= toMonth; ++month) {
        const auto monthPath = yearPath / format("{:02}", month);
        if (!std::filesystem::is_directory(monthPath)) {
          continue;
        }
        const bool isMonthFromExtremity = isYearFromExtremity && month == fromMonth;
        const bool isMonthToExtremity = isYearToExtremity && month == toMonth;
        const auto fromDay = isMonthFromExtremity ? static_cast<unsigned int>(fromYmd.day()) : 1U;
        const auto toDay = isMonthToExtremity ? static_cast<unsigned int>(toYmd.day()) : 31U;
        for (auto day = fromDay; day <= toDay; ++day) {
          const auto dayPath = monthPath / format("{:02}", day);
          if (!std::filesystem::is_directory(dayPath)) {
            continue;
          }

          const bool isDayFromExtremity = isMonthFromExtremity && day == fromDay;
          const bool isDayToExtremity = isMonthToExtremity && day == toDay;
          const auto fromHour = isDayFromExtremity ? static_cast<int>(fromTime.hours().count()) : 0;
          const auto toHour = isDayToExtremity ? static_cast<int>(toTime.hours().count()) : 23;
          for (auto hourOfDay = fromHour; hourOfDay <= toHour; ++hourOfDay) {
            const auto hourPath = dayPath / ComputeProtoFileName(hourOfDay);
            if (!std::filesystem::exists(hourPath)) {
              continue;
            }

            std::ifstream ifs(hourPath, std::ios::in | std::ios::binary);
            ProtobufMessagesReader multipleProtobufMessagesReader(ifs);
            while (multipleProtobufMessagesReader.hasNext()) {
              auto msg = multipleProtobufMessagesReader.next<ProtobufObjType>();
              if (!ValidateTimestamp(msg, timeWindow)) {
                continue;
              }
              ret.second = true;
              if (actionType == ActionType::kCheckPresence) {
                return ret;
              }
              ret.first.push_back(converter(std::move(msg)));
            }
          }
        }
      }
    }
    return ret;
  }

  std::string _exchangeSerializedDataPath;
};

}  // namespace cct
