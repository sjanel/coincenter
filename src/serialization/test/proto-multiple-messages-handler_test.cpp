#include "proto-multiple-messages-handler.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>

#include "cct_exception.hpp"
#include "proto-constants.hpp"
#include "proto-public-trade-converter.hpp"
#include "proto-test-data.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"
#include "serialization-tools.hpp"

namespace cct {

namespace {

class ProtobufMessagesTest : public ProtobufBaseDataTest {
 protected:
  PublicTradeConverter publicTradeConverter{mk1};

  ProtobufMessagesCompressedWriter<std::stringstream> writer;
};

class ProtobufMessagesCompressionTest : public ProtobufMessagesTest,
                                        public ::testing::WithParamInterface<ProtobufCompression> {
 protected:
  static int CountMessages(std::stringstream& ss, const PublicTrade& expectedTrade, PublicTradeConverter& converter) {
    ProtobufMessageCompressedReaderIterator reader{ss};
    int nbObjectsRead = 0;
    while (reader.hasNext()) {
      auto nextObj = reader.next<::proto::PublicTrade>();
      EXPECT_EQ(converter(nextObj), expectedTrade);
      ++nbObjectsRead;
    }
    return nbObjectsRead;
  }
};

}  // namespace

TEST_F(ProtobufMessagesTest, DefaultConstruction) {}

TEST_F(ProtobufMessagesTest, OpenShouldBeCalledBeforeWrite) { EXPECT_THROW(writer.write(td1), exception); }

TEST_F(ProtobufMessagesTest, WriteReadSingle) {
  writer.open(std::stringstream{});
  writer.write(td1);

  std::stringstream ss = writer.flush();

  ProtobufMessageCompressedReaderIterator reader{ss};

  int nbObjectsRead = 0;

  while (reader.hasNext()) {
    auto nextObj = reader.next<::proto::PublicTrade>();
    PublicTrade pt = publicTradeConverter(nextObj);

    EXPECT_EQ(pt, pt1);
    ++nbObjectsRead;
  }

  EXPECT_EQ(nbObjectsRead, 1);
}

TEST_F(ProtobufMessagesTest, WriteRead2Flushes) {
  writer.open(std::stringstream{});
  writer.write(td1);

  std::stringstream ss1 = writer.flush();

  writer.open(std::stringstream{});
  writer.write(td2);

  std::stringstream ss2 = writer.flush();

  ProtobufMessageCompressedReaderIterator reader1{ss1};

  int nbObjectsRead = 0;

  while (reader1.hasNext()) {
    auto nextObj = reader1.next<::proto::PublicTrade>();
    PublicTrade pt = publicTradeConverter(nextObj);

    EXPECT_EQ(pt, pt1);
    ++nbObjectsRead;
  }

  EXPECT_EQ(nbObjectsRead, 1);

  ProtobufMessageCompressedReaderIterator reader2{ss2};

  while (reader2.hasNext()) {
    auto nextObj = reader2.next<::proto::PublicTrade>();
    PublicTrade pt = publicTradeConverter(nextObj);

    EXPECT_EQ(pt, pt2);
    ++nbObjectsRead;
  }

  EXPECT_EQ(nbObjectsRead, 2);
}

TEST_F(ProtobufMessagesTest, WriteReadSeveral) {
  writer.open(std::stringstream{});
  writer.write(td1);
  writer.write(td2);
  writer.write(td3);

  std::stringstream ss = writer.flush();

  ProtobufMessageCompressedReaderIterator reader{ss};

  int nbObjectsRead = 0;

  while (reader.hasNext()) {
    auto nextObj = reader.next<::proto::PublicTrade>();
    PublicTrade pt = publicTradeConverter(nextObj);

    switch (nbObjectsRead) {
      case 0:
        EXPECT_EQ(pt, pt1);
        break;
      case 1:
        EXPECT_EQ(pt, pt2);
        break;
      case 2:
        EXPECT_EQ(pt, pt3);
        break;
      default:
        break;
    }

    ++nbObjectsRead;
  }

  EXPECT_EQ(nbObjectsRead, 3);
}

TEST_P(ProtobufMessagesCompressionTest, WriteReadSeveral) {
  writer.open(std::stringstream{}, GetParam());
  writer.write(td1);
  writer.write(td2);
  writer.write(td3);

  std::stringstream ss = writer.flush();

  EXPECT_EQ(DetectProtobufCompression(ss), GetParam());

  ProtobufMessageCompressedReaderIterator reader{ss};

  const PublicTrade expected[] = {pt1, pt2, pt3};
  std::size_t nbObjectsRead = 0;
  while (reader.hasNext()) {
    auto nextObj = reader.next<::proto::PublicTrade>();
    ASSERT_LT(nbObjectsRead, std::size(expected));
    EXPECT_EQ(publicTradeConverter(nextObj), expected[nbObjectsRead]);
    ++nbObjectsRead;
  }
  EXPECT_EQ(nbObjectsRead, std::size(expected));
}

TEST_P(ProtobufMessagesCompressionTest, WriteReadManyMessagesLargerThanStreamBuffers) {
  static constexpr int kNbMessages = 200000;

  writer.open(std::stringstream{}, GetParam());
  for (int pos = 0; pos < kNbMessages; ++pos) {
    writer.write(td1);
  }

  std::stringstream ss = writer.flush();

  EXPECT_EQ(CountMessages(ss, pt1, publicTradeConverter), kNbMessages);
}

TEST_P(ProtobufMessagesCompressionTest, EmptyFileHasNoMessage) {
  writer.open(std::stringstream{}, GetParam());

  std::stringstream ss = writer.flush();

  EXPECT_EQ(CountMessages(ss, pt1, publicTradeConverter), 0);
}

TEST_P(ProtobufMessagesCompressionTest, SeveralConcatenatedStreamsAreReadAsOne) {
  // Appending to an existing file produces concatenated compressed streams
  writer.open(std::stringstream{}, GetParam());
  writer.write(td1);
  writer.write(td1);
  std::stringstream ss1 = writer.flush();

  writer.open(std::stringstream{}, GetParam());
  writer.write(td1);
  std::stringstream ss2 = writer.flush();

  std::stringstream concatenated(ss1.str() + ss2.str());

  EXPECT_EQ(CountMessages(concatenated, pt1, publicTradeConverter), 3);
}

TEST_P(ProtobufMessagesCompressionTest, CompressedSmallerThanRaw) {
  static constexpr int kNbMessages = 1000;

  writer.open(std::stringstream{}, GetParam());
  for (int pos = 0; pos < kNbMessages; ++pos) {
    writer.write(td1);
  }
  const std::string compressed = writer.flush().str();

  EXPECT_LT(compressed.size(), static_cast<std::size_t>(kNbMessages) * td1.ByteSizeLong() / 4);
}

INSTANTIATE_TEST_SUITE_P(Compressions, ProtobufMessagesCompressionTest,
                         ::testing::Values(ProtobufCompression::kGzip, ProtobufCompression::kZstd),
                         [](const ::testing::TestParamInfo<ProtobufCompression>& info) {
                           return info.param == ProtobufCompression::kGzip ? "Gzip" : "Zstd";
                         });

TEST_F(ProtobufMessagesTest, ZstdIsDefaultCompression) {
  writer.open(std::stringstream{});
  writer.write(td1);

  std::stringstream ss = writer.flush();

  EXPECT_EQ(kDefaultProtobufCompression, ProtobufCompression::kZstd);
  EXPECT_EQ(DetectProtobufCompression(ss), ProtobufCompression::kZstd);
}

TEST_F(ProtobufMessagesTest, TruncatedZstdDataDoesNotCrash) {
  writer.open(std::stringstream{}, ProtobufCompression::kZstd);
  for (int pos = 0; pos < 1000; ++pos) {
    writer.write(td1);
  }
  std::string data = writer.flush().str();
  data.resize(data.size() / 2);

  std::stringstream ss(data);
  ProtobufMessageCompressedReaderIterator reader{ss};

  int nbObjectsRead = 0;
  while (reader.hasNext()) {
    reader.next<::proto::PublicTrade>();
    ++nbObjectsRead;
  }
  EXPECT_LT(nbObjectsRead, 1000);
}

}  // namespace cct
