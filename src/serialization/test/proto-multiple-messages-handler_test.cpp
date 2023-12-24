#include "proto-multiple-messages-handler.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "cct_exception.hpp"
#include "proto-public-trade-converter.hpp"
#include "proto-test-data.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"

namespace cct {

class ProtobufMessagesTest : public ProtobufBaseDataTest {
 protected:
  PublicTradeConverter publicTradeConverter{mk1};

  ProtobufMessagesCompressedWriter<std::stringstream> writer;
};

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

}  // namespace cct
