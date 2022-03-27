#include "wallet.hpp"

#include <gtest/gtest.h>

namespace cct {

class WalletTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  WalletCheck walletCheck;
};

TEST_F(WalletTest, NoDestinationTag) {
  Wallet w(ExchangeName("kraken", "user1"), "ETH", "MyAddress", "", walletCheck);
  EXPECT_EQ(w.address(), "MyAddress");
  EXPECT_FALSE(w.hasTag());
  EXPECT_EQ(w.tag(), "");
  EXPECT_EQ(w.currencyCode(), CurrencyCode("ETH"));
}

TEST_F(WalletTest, DestinationTag1) {
  Wallet w(ExchangeName("kraken", "user1"), "ETH", "023432423423xxxx54645654", "346723423", walletCheck);
  EXPECT_EQ(w.address(), "023432423423xxxx54645654");
  EXPECT_TRUE(w.hasTag());
  EXPECT_EQ(w.tag(), "346723423");
}

TEST_F(WalletTest, DestinationTag2) {
  string address("023432423423xxxx5464565sd234657dsfsdfnnMMSERwedfsas");
  Wallet w(ExchangeName("kraken", "user1"), "XRP", std::move(address), "superTAG4576", walletCheck);
  EXPECT_EQ(w.address(), "023432423423xxxx5464565sd234657dsfsdfnnMMSERwedfsas");
  EXPECT_TRUE(w.hasTag());
  EXPECT_EQ(w.tag(), "superTAG4576");
}

}  // namespace cct