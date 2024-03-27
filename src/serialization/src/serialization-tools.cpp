#include "serialization-tools.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "proto-constants.hpp"

namespace cct {

std::string ComputeProtoSubPath(std::string_view dataDir, std::string_view exchangeName,
                                std::string_view protobufObjectName) {
  std::string ret;

  static constexpr std::string_view kSerializedDataSubPath = "/serialized/";

  ret.reserve(dataDir.size() + kSerializedDataSubPath.size() + protobufObjectName.size() + exchangeName.size() + 1U);

  ret.append(dataDir);
  ret.append(kSerializedDataSubPath);
  ret.append(protobufObjectName);
  ret.push_back('/');
  ret.append(exchangeName);
  return ret;
}

namespace {

consteval auto BuildBinProtoFileNames() {
  constexpr std::string_view kBinProtoFilePart2 = ":00:00_";
  constexpr std::string_view kBinProtoFilePart3 = ":59:59";

  using ProtoFileNameBuffer =
      std::array<char, kBinProtobufExtension.size() + kBinProtoFilePart2.size() + kBinProtoFilePart3.size() +
                           static_cast<std::string_view::size_type>(2 * 2)>;

  constexpr auto kNbHourInDay = 24;

  std::array<ProtoFileNameBuffer, kNbHourInDay> ret;

  for (auto hourOfDay = 0; hourOfDay < kNbHourInDay; ++hourOfDay) {
    std::array hourStr = {static_cast<char>((hourOfDay / 10) + '0'), static_cast<char>((hourOfDay % 10) + '0')};

    auto it = ret[hourOfDay].begin();

    it = std::ranges::copy(hourStr, it).out;
    it = std::ranges::copy(kBinProtoFilePart2, it).out;
    it = std::ranges::copy(hourStr, it).out;
    it = std::ranges::copy(kBinProtoFilePart3, it).out;
    it = std::ranges::copy(kBinProtobufExtension, it).out;
  }

  return ret;
}

}  // namespace

std::string_view ComputeProtoFileName(int hourOfDay) {
  static constexpr auto kBinProtoFileNames = BuildBinProtoFileNames();

  const auto &protoFileName = kBinProtoFileNames[hourOfDay];

  return {protoFileName.data(), protoFileName.size()};
}

}  // namespace cct