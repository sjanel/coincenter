#include "serialization-tools.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>

#include "proto-constants.hpp"

namespace cct {

std::filesystem::path ComputeProtoSubPath(std::string_view dataDir, std::string_view exchangeName,
                                          std::string_view protobufObjectName) {
  std::filesystem::path ret(dataDir);

  ret /= "serialized";
  ret /= protobufObjectName;
  ret /= exchangeName;

  return ret;
}

namespace {

consteval auto BuildIntLessThan31Strings() {
  // 31 days
  constexpr auto kNbMaxUnit = 31;

  std::array<std::array<char, 2>, kNbMaxUnit> ret;

  for (auto unit = 1; unit <= kNbMaxUnit; ++unit) {
    ret[unit - 1][0] = (unit / 10) + '0';
    ret[unit - 1][1] = (unit % 10) + '0';
  }

  return ret;
}

constexpr auto kIntLessThan31Strings = BuildIntLessThan31Strings();

}  // namespace

std::string_view MonthStr(int month) { return std::string_view{kIntLessThan31Strings[month - 1]}; }

std::string_view DayOfMonthStr(int dayOfMonth) { return std::string_view{kIntLessThan31Strings[dayOfMonth - 1]}; }

namespace {

consteval auto BuildBinProtoFileNames() {
  // We use '-' instead of ':' as hour / minutes separator as some file systems do not accept this character in a
  // filename (Windows for instance)
  constexpr std::string_view kBinProtoFileHourBegPart = "-00-00_";
  constexpr std::string_view kBinProtoFileHourEndPart = "-59-59";

  using ProtoFileNameBuffer =
      std::array<char, kBinProtobufExtension.size() + kBinProtoFileHourBegPart.size() +
                           kBinProtoFileHourEndPart.size() + static_cast<std::string_view::size_type>(2 * 2)>;

  constexpr auto kNbHourInDay = 24;

  std::array<ProtoFileNameBuffer, kNbHourInDay> ret;

  for (auto hourOfDay = 0; hourOfDay < kNbHourInDay; ++hourOfDay) {
    std::array hourStr = {static_cast<char>((hourOfDay / 10) + '0'), static_cast<char>((hourOfDay % 10) + '0')};

    auto it = ret[hourOfDay].begin();

    it = std::ranges::copy(hourStr, it).out;
    it = std::ranges::copy(kBinProtoFileHourBegPart, it).out;
    it = std::ranges::copy(hourStr, it).out;
    it = std::ranges::copy(kBinProtoFileHourEndPart, it).out;
    it = std::ranges::copy(kBinProtobufExtension, it).out;
  }

  return ret;
}

}  // namespace

std::string_view ComputeProtoFileName(int hourOfDay) {
  static constexpr auto kBinProtoFileNames = BuildBinProtoFileNames();

  return std::string_view{kBinProtoFileNames[hourOfDay]};
}

}  // namespace cct