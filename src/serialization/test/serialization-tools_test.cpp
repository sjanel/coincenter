#include "serialization-tools.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>

#include "proto-constants.hpp"

namespace cct {

TEST(SerializationTools, ComputeProtoSubPath) {
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "upbit", "order-books"), "/path/to/data/serialized/order-books/upbit");
  EXPECT_EQ(ComputeProtoSubPath("another-path", "bithumb", "trades"), "another-path/serialized/trades/bithumb");
  EXPECT_EQ(ComputeProtoSubPath(".", "kraken", "trades"), "./serialized/trades/kraken");
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "binance", "order-books"),
            "/path/to/data/serialized/order-books/binance");
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "kucoin", "trades"), "/path/to/data/serialized/trades/kucoin");
}

TEST(SerializationTools, MonthStr) {
  EXPECT_EQ(MonthStr(1), "01");
  EXPECT_EQ(MonthStr(6), "06");
  EXPECT_EQ(MonthStr(10), "10");
  EXPECT_EQ(MonthStr(11), "11");
  EXPECT_EQ(MonthStr(12), "12");
}

TEST(SerializationTools, DayOfMonthStr) {
  EXPECT_EQ(DayOfMonthStr(1), "01");
  EXPECT_EQ(DayOfMonthStr(2), "02");
  EXPECT_EQ(DayOfMonthStr(10), "10");
  EXPECT_EQ(DayOfMonthStr(17), "17");
  EXPECT_EQ(DayOfMonthStr(22), "22");
  EXPECT_EQ(DayOfMonthStr(31), "31");
}

TEST(SerializationTools, ComputeProtoFileName) {
  EXPECT_EQ(ComputeProtoFileName(0), "00-00-00_00-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(4), "04-00-00_04-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(17), "17-00-00_17-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(23), "23-00-00_23-59-59.binpb");
}

TEST(SerializationTools, DetectProtobufCompressionFromStream) {
  std::stringstream gzipStream(std::string("\x1f\x8b\x08\x00", 4));
  EXPECT_EQ(DetectProtobufCompression(gzipStream), ProtobufCompression::kGzip);
  // detection does not consume the stream
  EXPECT_EQ(gzipStream.get(), 0x1f);

  std::stringstream zstdStream(std::string("\x28\xb5\x2f\xfd", 4));
  EXPECT_EQ(DetectProtobufCompression(zstdStream), ProtobufCompression::kZstd);
  EXPECT_EQ(zstdStream.get(), 0x28);

  std::stringstream emptyStream;
  EXPECT_EQ(DetectProtobufCompression(emptyStream), ProtobufCompression::kGzip);
}

TEST(SerializationTools, DetectProtobufFileCompression) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "cct-detect-compression-test";
  std::filesystem::create_directories(dir);

  const auto write = [&dir](std::string_view name, std::string_view content) {
    std::filesystem::path path = dir / name;
    std::ofstream(path, std::ios::binary) << content;
    return path;
  };

  const auto gzipPath = write("gzip", std::string_view("\x1f\x8b\x08\x00", 4));
  const auto zstdPath = write("zstd", std::string_view("\x28\xb5\x2f\xfd", 4));
  const auto emptyPath = write("empty", "");

  for (ProtobufCompression fallback : {ProtobufCompression::kGzip, ProtobufCompression::kZstd}) {
    EXPECT_EQ(DetectProtobufFileCompression(gzipPath, fallback), ProtobufCompression::kGzip);
    EXPECT_EQ(DetectProtobufFileCompression(zstdPath, fallback), ProtobufCompression::kZstd);
    EXPECT_EQ(DetectProtobufFileCompression(emptyPath, fallback), fallback);
    EXPECT_EQ(DetectProtobufFileCompression(dir / "missing", fallback), fallback);
  }

  std::filesystem::remove_all(dir);
}

}  // namespace cct