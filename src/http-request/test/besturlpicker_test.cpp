#include "besturlpicker.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace {
constexpr std::string_view kSingleURL = "singleurl";
constexpr std::string_view kSeveralURL[] = {"url1", "url2", "url3"};
}  // namespace

TEST(BestURLPicker, SingleURL) {
  BestURLPicker bestURLPicker(kSingleURL);

  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSingleURL);
  EXPECT_EQ(bestURLPicker.nbBaseURL(), 1);

  for (int requestPos = 0; requestPos < 20; ++requestPos) {
    // Whatever the response time stats are, we should always return the unique URL that we stored.
    EXPECT_EQ(bestURLPicker.nextBaseURLPos(), 0);
    bestURLPicker.storeResponseTimePerBaseURL(0, requestPos * 10);
  }
}

TEST(BestURLPicker, SeveralURL) {
  BestURLPicker bestURLPicker(kSeveralURL);

  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  EXPECT_EQ(bestURLPicker.nbBaseURL(), std::end(kSeveralURL) - std::begin(kSeveralURL));

  // Store some response times

  // For url 0, avg = 29, dev = 27.386127875
  bestURLPicker.storeResponseTimePerBaseURL(0, 30);
  bestURLPicker.storeResponseTimePerBaseURL(0, 24);
  bestURLPicker.storeResponseTimePerBaseURL(0, 37);
  bestURLPicker.storeResponseTimePerBaseURL(0, 36);
  bestURLPicker.storeResponseTimePerBaseURL(0, 32);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 15);
  bestURLPicker.storeResponseTimePerBaseURL(0, 22);
  bestURLPicker.storeResponseTimePerBaseURL(0, 19);
  bestURLPicker.storeResponseTimePerBaseURL(0, 45);
  bestURLPicker.storeResponseTimePerBaseURL(0, 30);

  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[1]);

  // For url 1, avg = 41, dev = 23.452078799
  bestURLPicker.storeResponseTimePerBaseURL(1, 35);
  bestURLPicker.storeResponseTimePerBaseURL(1, 35);
  bestURLPicker.storeResponseTimePerBaseURL(1, 37);
  bestURLPicker.storeResponseTimePerBaseURL(1, 62);
  bestURLPicker.storeResponseTimePerBaseURL(1, 41);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[1]);
  bestURLPicker.storeResponseTimePerBaseURL(1, 39);
  bestURLPicker.storeResponseTimePerBaseURL(1, 39);
  bestURLPicker.storeResponseTimePerBaseURL(1, 38);
  bestURLPicker.storeResponseTimePerBaseURL(1, 41);
  bestURLPicker.storeResponseTimePerBaseURL(1, 43);

  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);

  // For url 2, avg = 32, dev = 14.662878299
  bestURLPicker.storeResponseTimePerBaseURL(2, 27);
  bestURLPicker.storeResponseTimePerBaseURL(2, 27);
  bestURLPicker.storeResponseTimePerBaseURL(2, 29);
  bestURLPicker.storeResponseTimePerBaseURL(2, 44);
  bestURLPicker.storeResponseTimePerBaseURL(2, 33);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);
  bestURLPicker.storeResponseTimePerBaseURL(2, 31);
  bestURLPicker.storeResponseTimePerBaseURL(2, 31);
  bestURLPicker.storeResponseTimePerBaseURL(2, 30);
  bestURLPicker.storeResponseTimePerBaseURL(2, 33);
  bestURLPicker.storeResponseTimePerBaseURL(2, 35);

  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);

  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  bestURLPicker.storeResponseTimePerBaseURL(2, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[2]);

  // First URL should now be the best as we had good response times for the last queries
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);

  // we should give 10 % of requests the chance to the least used URLs
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[1]);
  bestURLPicker.storeResponseTimePerBaseURL(0, 28);
  EXPECT_EQ(bestURLPicker.getNextBaseURL(), kSeveralURL[0]);
}
}  // namespace cct