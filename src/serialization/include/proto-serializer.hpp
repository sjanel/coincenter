#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_vector.hpp"
#include "durationstring.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "proto-multiple-messages-handler.hpp"
#include "serialization-tools.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"

namespace cct {

/// Class responsible to accumulate protobuf objects in memory and perform regular flushes of its data to the disk.
/// Data is accumulated by Market and will write to following files (from subPath):
///  'BASECUR-QUOTECUR/YYYY/MM/DD/HH:00:00_HH:59:59.binpb'
///
/// If you may push duplicated objects, you have to provide Comp and Equal types.
/// In this case, Equal must be consistent with Comp, and the first criteria of the comparison should be the timestamp
/// (ordered from oldest to youngest).
///
/// You may not provide any Comp and Equal if by design you will not push duplicated data.
template <class ProtobufObjectType, class Comp = void, class Equal = void, int32_t RehashThreshold = 1000,
          class DurationType = std::chrono::days, int32_t DurationValue = 3>
class ProtobufObjectsSerializer {
 public:
  /// Creates a new ProtobufObjectsSerializer.
  /// @param marketTimestampSet the latest written timestamp for all markets to avoid writing duplicate entries between
  /// coincenter restarts.
  ProtobufObjectsSerializer(std::filesystem::path subPath, const MarketTimestampSet &marketTimestampSet,
                            int32_t nbObjectsPerMarketInMemory)
      : _subPath(std::move(subPath)), _nbObjectsPerMarketInMemory(nbObjectsPerMarketInMemory) {
    for (const auto &[market, timestamp] : marketTimestampSet) {
      auto &lastWrittenObjectTimestamp = _marketDataMap[market].lastWrittenObjectTimestamp;

      lastWrittenObjectTimestamp = timestamp;

      // When program starts, we want to exclude equal timestamps to avoid writing objects that may have been written
      // already from a previous run (the SortUnique will not protect us here)
      ++lastWrittenObjectTimestamp;
    }
  }

  ProtobufObjectsSerializer(const ProtobufObjectsSerializer &) = delete;
  ProtobufObjectsSerializer &operator=(const ProtobufObjectsSerializer &) = delete;

  ProtobufObjectsSerializer(ProtobufObjectsSerializer &&other) noexcept { swap(other); }

  ProtobufObjectsSerializer &operator=(ProtobufObjectsSerializer &&other) noexcept {
    if (&other != this) {
      swap(other);
    }
    return *this;
  }

  /// At destruction of the serializer, we try to write all remaining objects in the buffer (as best effort mode).
  ~ProtobufObjectsSerializer() {
    try {
      for (auto &[market, marketData] : _marketDataMap) {
        writeOnDisk(market, marketData);
      }
    } catch (const std::exception &e) {
      log::error("exception caught in writeOnDisk at ProtobufObjectsSerializer destruction: {}", e.what());
    }
  }

  /// Pushes a new object into the serializer.
  /// The new object is guaranteed to be written upon destruction of this serializer at the latest unless:
  ///  - its timestamp is older than the latest written timestamp of this market
  ///  - it has invalid data
  template <class ProtobufObjectTypeU>
  void push(Market market, ProtobufObjectTypeU &&protoObj) {
    if (!protoObj.has_unixtimestampinms()) {
      throw exception("Attempt to push proto object without any timestamp");
    }

    auto &marketData = _marketDataMap[market];
    if (TimePoint{milliseconds{protoObj.unixtimestampinms()}} < marketData.lastWrittenObjectTimestamp) {
      // do not push an object that has an older timestamp of the last written object
      return;
    }

    marketData.dataVector.push_back(std::forward<ProtobufObjectTypeU>(protoObj));

    checkWriteOnDisk(market, marketData);
  }

  void swap(ProtobufObjectsSerializer &rhs) noexcept {
    _marketDataMap.swap(rhs._marketDataMap);
    _subPath.swap(rhs._subPath);
    std::swap(_nbObjectsPerMarketInMemory, rhs._nbObjectsPerMarketInMemory);
    std::swap(_flushCounter, rhs._flushCounter);
  }

 private:
  using ProtobufObjectTypeVector = vector<ProtobufObjectType>;

  struct MarketData {
    ProtobufObjectTypeVector dataVector;
    TimePoint lastWrittenObjectTimestamp;
  };

  void checkWriteOnDisk(Market market, MarketData &marketData) {
    auto &dataVector = marketData.dataVector;
    if (dataVector.size() == static_cast<ProtobufObjectTypeVector::size_type>(_nbObjectsPerMarketInMemory)) {
      writeOnDisk(market, marketData);

      // shrink_to_fit as vector will never grow-up larger than its current size
      dataVector.shrink_to_fit();
      dataVector.clear();

      checkPeriodicFlush();
    }
  }

  void writeOnDisk(Market market, MarketData &marketData) {
    auto &dataVector = marketData.dataVector;
    if (dataVector.empty()) {
      return;
    }

    const auto nowTime = std::chrono::steady_clock::now();

    SortUnique(dataVector);

    std::filesystem::path path;

    std::chrono::hours prevHourOfDay{-1};

    ProtobufMessagesCompressedWriter<std::ofstream> protobufMessagesWriter;

    for (const auto &protobufObject : dataVector) {
      checkOpenFile(market, protobufObject, prevHourOfDay, path, protobufMessagesWriter);

      protobufMessagesWriter.write(protobufObject);
    }

    marketData.lastWrittenObjectTimestamp = TimePoint{milliseconds{dataVector.back().unixtimestampinms()}};

    const auto nbElemsWritten = dataVector.size();

    const auto steadyClockDuration = std::chrono::steady_clock::now() - nowTime;
    const auto dur = std::chrono::duration_cast<Duration>(steadyClockDuration);

    log::info("Serialized {} object(s) for {} data in {}, last in {}", nbElemsWritten, market, DurationToString(dur),
              path.string());
  }

  // Periodic memory release to avoid possible leaks for long time running (if market data unused anymore for instance)
  void checkPeriodicFlush() {
    if (++_flushCounter != RehashThreshold) {
      return;
    }

    _flushCounter = 0;

    auto nowTime = Clock::now();

    for (auto it = _marketDataMap.begin(); it != _marketDataMap.end();) {
      if (it->second.lastWrittenObjectTimestamp + DurationType{static_cast<int64_t>(DurationValue)} < nowTime) {
        // Unchanged data since a long time - write data if any, and clears the entry in the map
        const Market market = it->first;
        MarketData &marketData = it->second;

        writeOnDisk(market, marketData);

        log::info("Released {} protobuf objects for {}", marketData.dataVector.capacity(), market);

        it = _marketDataMap.erase(it);
      } else {
        ++it;
      }
    }

    _marketDataMap.rehash(_marketDataMap.size());
  }

  static void SortUnique(ProtobufObjectTypeVector &dataVector) {
    static_assert((std::is_void_v<Comp> && std::is_void_v<Equal>) || (!std::is_void_v<Comp> && !std::is_void_v<Equal>));

    if constexpr (std::is_void_v<Comp>) {
      // Sort by timestamp (required by 'writeOnDisk' algorithm)
      std::ranges::sort(dataVector, [](const auto &lhs, const auto &rhs) {
        return lhs.unixtimestampinms() < rhs.unixtimestampinms();
      });
    } else {
      // We assume that timestamp is the first sorting criteria
      std::ranges::sort(dataVector, Comp{});
    }

    // If duplicate elements are possible, remove them
    if constexpr (!std::is_void_v<Comp>) {
      const auto [eraseIt1, eraseIt2] = std::ranges::unique(dataVector, Equal{});

      dataVector.erase(eraseIt1, eraseIt2);
    }
  }

  void checkOpenFile(Market market, const ProtobufObjectType &protobufObject, std::chrono::hours &prevHourOfDay,
                     std::filesystem::path &path,
                     ProtobufMessagesCompressedWriter<std::ofstream> &protobufMessagesWriter) {
    const TimePoint tp{milliseconds{protobufObject.unixtimestampinms()}};
    const auto hourOfDay = GetHourOfDay(tp);

    if (prevHourOfDay != hourOfDay) {
      // open new outfile
      setDirectory(market.str(), tp, path);

      std::filesystem::create_directories(path);

      path /= ComputeProtoFileName(std::chrono::duration_cast<std::chrono::hours>(hourOfDay).count());

      std::ofstream ofs(path, std::ios_base::out | std::ios::binary | std::ios_base::app);

      if (!ofs.is_open()) {
        throw exception("Cannot open the ofstream for writing to {}: {} (code {})", path.string(), std::strerror(errno),
                        errno);
      }

      protobufMessagesWriter.open(std::move(ofs));
      prevHourOfDay = hourOfDay;
    }
  }

  static std::chrono::hours GetHourOfDay(TimePoint tp) {
    const auto dp = std::chrono::floor<std::chrono::days>(tp);

    return std::chrono::floor<std::chrono::hours>(tp - dp);
  }

  void setDirectory(std::string_view marketStr, TimePoint tp, std::filesystem::path &path) {
    const auto dp = std::chrono::floor<std::chrono::days>(tp);
    const std::chrono::year_month_day ymd{dp};

    path = _subPath / marketStr;
    path /= std::string_view(IntegralToCharVector(static_cast<int>(ymd.year())));
    path /= MonthStr(static_cast<unsigned int>(ymd.month()));
    path /= DayOfMonthStr(static_cast<unsigned int>(ymd.day()));
  }

  using MarketDataMap = std::unordered_map<Market, MarketData>;

  MarketDataMap _marketDataMap;
  std::filesystem::path _subPath;
  int32_t _nbObjectsPerMarketInMemory;
  int32_t _flushCounter{};
};

}  // namespace cct
