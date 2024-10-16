#include "deposit-addresses.hpp"

#include <gtest/gtest.h>

#include "cct_string.hpp"
#include "read-json.hpp"
#include "reader.hpp"

namespace cct::schema {

TEST(DepositAddressesTest, NominalCase) {
  class NominalCase : public Reader {
    [[nodiscard]] string readAll() const override {
      return R"(
{
  "binance": {
    "user1": {
      "EUR": "0x1234567890abcde1",
      "DOGE": "D123456789"
    }
  },
  "kraken": {
    "user1": {
      "EUR": "0x1234567890abcdefg2",
      "DOGE": "D123456789"
    },
    "user2": {
      "EUR": "0x1234567890abcdef3",
      "ETH": "0xETHaddress"
    }
  }
}
)";
    }
  };

  DepositAddresses depositAddresses = ReadJsonOrThrow<DepositAddresses>(NominalCase{});

  EXPECT_EQ(depositAddresses.size(), 2);
  EXPECT_EQ(depositAddresses.at("binance").size(), 1);
  EXPECT_EQ(depositAddresses.at("kraken").size(), 2);
  EXPECT_EQ(depositAddresses.at("binance").at("user1").size(), 2);
  EXPECT_EQ(depositAddresses.at("kraken").at("user1").size(), 2);
  EXPECT_EQ(depositAddresses.at("kraken").at("user2").size(), 2);
  EXPECT_EQ(depositAddresses.at("binance").at("user1").at("EUR"), "0x1234567890abcde1");
  EXPECT_EQ(depositAddresses.at("binance").at("user1").at("DOGE"), "D123456789");
  EXPECT_EQ(depositAddresses.at("kraken").at("user1").at("EUR"), "0x1234567890abcdefg2");
  EXPECT_EQ(depositAddresses.at("kraken").at("user1").at("DOGE"), "D123456789");
  EXPECT_EQ(depositAddresses.at("kraken").at("user2").at("EUR"), "0x1234567890abcdef3");
  EXPECT_EQ(depositAddresses.at("kraken").at("user2").at("ETH"), "0xETHaddress");
}

}  // namespace cct::schema