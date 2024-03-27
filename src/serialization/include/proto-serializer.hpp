#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "cct_log.hpp"
#include "cct_vector.hpp"
#include "durationstring.hpp"
#include "market.hpp"
#include "proto-multiple-messages-handler.hpp"
#include "serialization-tools.hpp"
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
template <class ProtobufObjectType, class Comp = void, class Equal = void>
class ProtobufObjectsSerializer {
 public:
  static_assert((std::is_void_v<Comp> && std::is_void_v<Equal>) || (!std::is_void_v<Comp> && !std::is_void_v<Equal>));

  using ProtobufObjectTypeVector = vector<ProtobufObjectType>;
  using ProtoObjectTypePerMarketMap = std::unordered_map<Market, ProtobufObjectTypeVector>;

  explicit ProtobufObjectsSerializer(std::string subPath, int32_t nbObjectsPerMarketInMemory) noexcept
      : _subPath(std::move(subPath)), _nbObjectsPerMarketInMemory(nbObjectsPerMarketInMemory) {}

  ProtobufObjectsSerializer(const ProtobufObjectsSerializer &) = delete;
  ProtobufObjectsSerializer &operator=(const ProtobufObjectsSerializer &) = delete;

  ProtobufObjectsSerializer(ProtobufObjectsSerializer &&other) noexcept { swap(other); }

  ProtobufObjectsSerializer &operator=(ProtobufObjectsSerializer &&other) noexcept {
    if (&other != this) {
      swap(other);
    }
    return *this;
  }

  ~ProtobufObjectsSerializer() {
    try {
      for (auto &[market, protobufObjectsVector] : _protoObjectsPerMarketMap) {
        writeOnDisk(market, protobufObjectsVector, MemoryReleaseAllowed::kNo);
      }
    } catch (const std::exception &e) {
      log::error("exception caught in writeOnDisk: {}", e.what());
    }
  }

  template <class ProtobufObjectTypeU>
  void push(Market market, ProtobufObjectTypeU &&protoObj) {
    if (!isValid(protoObj)) {
      return;
    }

    auto &protobufObjectsVector = _protoObjectsPerMarketMap[market];
    protobufObjectsVector.push_back(std::forward<ProtobufObjectTypeU>(protoObj));

    checkWriteOnDisk(market, protobufObjectsVector);
  }

  void swap(ProtobufObjectsSerializer &rhs) noexcept {
    _protoObjectsPerMarketMap.swap(rhs._protoObjectsPerMarketMap);
    _subPath.swap(rhs._subPath);
    std::swap(_lastProtoObjWrittenUnixTimeStampInMs, rhs._lastProtoObjWrittenUnixTimeStampInMs);
    std::swap(_nbObjectsPerMarketInMemory, rhs._nbObjectsPerMarketInMemory);
    std::swap(_nbFlushes, rhs._nbFlushes);
  }

 private:
  enum class MemoryReleaseAllowed : int8_t { kYes, kNo };

  void checkWriteOnDisk(Market market, ProtobufObjectTypeVector &protobufObjectsVector) {
    if (protobufObjectsVector.size() == static_cast<ProtobufObjectTypeVector::size_type>(_nbObjectsPerMarketInMemory)) {
      writeOnDisk(market, protobufObjectsVector, MemoryReleaseAllowed::kYes);
    }
  }

  void writeOnDisk(Market market, ProtobufObjectTypeVector &protobufObjectsVector,
                   MemoryReleaseAllowed periodicMemoryRelease) {
    if (protobufObjectsVector.empty()) {
      return;
    }

    const auto nowTime = std::chrono::steady_clock::now();

    SortUnique(protobufObjectsVector);

    std::string pathStr = _subPath;

    std::chrono::hours prevHourOfDay{-1};

    ProtobufMessagesWriter<std::ofstream> protobufMessagesWriter;
    for (const auto &protobufObject : protobufObjectsVector) {
      checkOpenFile(market, protobufObject, prevHourOfDay, pathStr, protobufMessagesWriter);

      protobufMessagesWriter.write(protobufObject);
    }

    _lastProtoObjWrittenUnixTimeStampInMs = protobufObjectsVector.back().unixtimestampinms();

    const auto nbElemsWritten = protobufObjectsVector.size();

    if (periodicMemoryRelease == MemoryReleaseAllowed::kYes) {
      static constexpr decltype(_nbFlushes) kRehashThreshold = 1000;
      if (++_nbFlushes == kRehashThreshold) {
        // Periodic memory release to avoid possible leaks for long time running (if market data unused anymore for
        // instance)
        _nbFlushes = 0;
        log::info("Released {} protobuf objects for {}", protobufObjectsVector.capacity(), market);
        _protoObjectsPerMarketMap.erase(market);
        _protoObjectsPerMarketMap.rehash(_protoObjectsPerMarketMap.size());
      } else {
        // shrink_to_fit as vector will never grow-up larger than its current size
        protobufObjectsVector.shrink_to_fit();
        protobufObjectsVector.clear();
      }
    }

    const auto steadyClockDuration = std::chrono::steady_clock::now() - nowTime;
    const Duration dur = std::chrono::duration_cast<Duration>(steadyClockDuration);

    log::info("Wrote {} objects for {} timed data in {}, last in {}", nbElemsWritten, market, DurationToString(dur),
              pathStr);
  }

  static void SortUnique(ProtobufObjectTypeVector &protobufObjectsVector) {
    if constexpr (std::is_void_v<Comp>) {
      // Sort by timestamp (required by 'writeOnDisk' algorithm)
      std::ranges::sort(protobufObjectsVector, [](const auto &lhs, const auto &rhs) {
        return lhs.unixtimestampinms() < rhs.unixtimestampinms();
      });
    } else {
      // We assume that timestamp is the first sorting criteria
      std::ranges::sort(protobufObjectsVector, Comp{});
    }

    // If duplicate elements are possible, remove them
    if constexpr (!std::is_void_v<Comp>) {
      const auto [eraseIt1, eraseIt2] = std::ranges::unique(protobufObjectsVector, Equal{});
      protobufObjectsVector.erase(eraseIt1, eraseIt2);
    }
  }

  void checkOpenFile(Market market, const ProtobufObjectType &protobufObject, std::chrono::hours &prevHourOfDay,
                     std::string &pathStr, ProtobufMessagesWriter<std::ofstream> &protobufMessagesWriter) {
    const TimePoint tp{milliseconds{protobufObject.unixtimestampinms()}};
    const auto hourOfDay = GetHourOfDay(tp);

    if (prevHourOfDay != hourOfDay) {
      // open new outfile
      setDirectory(market, tp, pathStr);
      std::filesystem::create_directories(std::filesystem::path(pathStr));

      pathStr.append(ComputeProtoFileName(std::chrono::duration_cast<std::chrono::hours>(hourOfDay).count()));

      std::filesystem::path filePath(pathStr);

      protobufMessagesWriter.open(std::ofstream(filePath, std::ios_base::app));
      prevHourOfDay = hourOfDay;
    }
  }

  static std::chrono::hours GetHourOfDay(TimePoint tp) {
    const auto dp = std::chrono::floor<std::chrono::days>(tp);

    return std::chrono::floor<std::chrono::hours>(tp - dp);
  }

  bool isValid(const ProtobufObjectType &protoObj) const {
    if (!protoObj.has_unixtimestampinms()) {
      throw exception("Attempt to push proto object without any timestamp");
    }
    if (protoObj.unixtimestampinms() < _lastProtoObjWrittenUnixTimeStampInMs) {
      // do not push an object that has an older timestamp of the last written object
      return false;
    }
    return true;
  }

  void setDirectory(Market market, TimePoint tp, std::string &pathStr) const {
    // Note: below code  could be simplified once compilers fully implement std::format and chrono C++20
    // libraries.
    const auto dp = std::chrono::floor<std::chrono::days>(tp);
    const std::chrono::year_month_day ymd{dp};

    pathStr.replace(pathStr.begin() + _subPath.size(), pathStr.end(),
                    format("/{}/{:04}/{:02}/{:02}/", market, static_cast<int>(ymd.year()),
                           static_cast<unsigned int>(ymd.month()), static_cast<unsigned int>(ymd.day())));
  }

  ProtoObjectTypePerMarketMap _protoObjectsPerMarketMap;
  std::string _subPath;
  int64_t _lastProtoObjWrittenUnixTimeStampInMs{};
  int32_t _nbObjectsPerMarketInMemory;
  int32_t _nbFlushes{};
};

}  // namespace cct
