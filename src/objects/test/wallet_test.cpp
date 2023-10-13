#include "wallet.hpp"

#include <gtest/gtest.h>

#include <utility>

#include "accountowner.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"

namespace cct {

class WalletTest : public ::testing::Test {
 protected:
  WalletCheck walletCheck;
};

TEST_F(WalletTest, NoDestinationTag) {
  Wallet wallet(ExchangeName("kraken", "user1"), "ETH", "MyAddress", "", walletCheck,
                AccountOwner("SmithJohn", "스미스존"));
  EXPECT_EQ(wallet.address(), "MyAddress");
  EXPECT_FALSE(wallet.hasTag());
  EXPECT_EQ(wallet.tag(), "");
  EXPECT_EQ(wallet.currencyCode(), CurrencyCode("ETH"));
}

TEST_F(WalletTest, DestinationTag1) {
  Wallet wallet(ExchangeName("kraken", "user1"), "ETH", "023432423423xxxx54645654", "346723423", walletCheck,
                AccountOwner("SmithJohn", "스미스존"));
  EXPECT_EQ(wallet.address(), "023432423423xxxx54645654");
  EXPECT_TRUE(wallet.hasTag());
  EXPECT_EQ(wallet.tag(), "346723423");
}

TEST_F(WalletTest, DestinationTag2) {
  string address("023432423423xxxx5464565sd234657dsfsdfnnMMSERwedfsas");
  Wallet wallet(ExchangeName("kraken", "user1"), "XRP", std::move(address), "superTAG4576", walletCheck,
                AccountOwner("SmithJohn", "스미스존"));
  EXPECT_EQ(wallet.address(), "023432423423xxxx5464565sd234657dsfsdfnnMMSERwedfsas");
  EXPECT_TRUE(wallet.hasTag());
  EXPECT_EQ(wallet.tag(), "superTAG4576");
}

}  // namespace cct