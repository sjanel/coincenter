#include "replay-algorithm-name-iterator.hpp"

#include <gtest/gtest.h>

#include <string_view>

#include "cct_exception.hpp"

namespace cct {
class ReplayAlgorithmNameIteratorTest : public ::testing::Test {
 protected:
  static constexpr std::string_view kInvalidAlgorithmNames[] = {"any", "so-what,"};
  static constexpr std::string_view kAlgorithmNames[] = {"any",  "so-what", "angry",
                                                         "bird", "Jack",    "a-more-complex algorithm Name"};
};

TEST_F(ReplayAlgorithmNameIteratorTest, AlgorithmNamesValidity) {
  EXPECT_THROW(ReplayAlgorithmNameIterator("", kInvalidAlgorithmNames), exception);
  EXPECT_NO_THROW(ReplayAlgorithmNameIterator("", kAlgorithmNames));
}

TEST_F(ReplayAlgorithmNameIteratorTest, IteratorWithAll) {
  ReplayAlgorithmNameIterator it("", kAlgorithmNames);

  int algorithmPos = 0;
  while (it.hasNext()) {
    auto next = it.next();

    switch (algorithmPos) {
      case 0:
        [[fallthrough]];
      case 1:
        [[fallthrough]];
      case 2:
        [[fallthrough]];
      case 3:
        [[fallthrough]];
      case 4:
        [[fallthrough]];
      case 5:
        EXPECT_EQ(next, kAlgorithmNames[algorithmPos]);
        break;
      default:
        throw exception("Unexpected number of algorithm names");
    }

    ++algorithmPos;
  }

  EXPECT_EQ(algorithmPos, 6);
}

TEST_F(ReplayAlgorithmNameIteratorTest, IteratorWithUniqueAlgorithmSpecified) {
  ReplayAlgorithmNameIterator it("so-What", kAlgorithmNames);

  int algorithmPos = 0;
  while (it.hasNext()) {
    auto next = it.next();

    switch (algorithmPos) {
      case 0:
        EXPECT_EQ(next, "so-What");
        break;
      default:
        throw exception("Unexpected number of algorithm names");
    }

    ++algorithmPos;
  }

  EXPECT_EQ(algorithmPos, 1);
}

TEST_F(ReplayAlgorithmNameIteratorTest, IteratorWithSpecifiedList) {
  ReplayAlgorithmNameIterator it("Jack,whatever,so-what,some-algorithmNameThatIsNotInAll,with spaces", kAlgorithmNames);

  int algorithmPos = 0;
  while (it.hasNext()) {
    auto next = it.next();

    switch (algorithmPos) {
      case 0:
        EXPECT_EQ(next, "Jack");
        break;
      case 1:
        EXPECT_EQ(next, "whatever");
        break;
      case 2:
        EXPECT_EQ(next, "so-what");
        break;
      case 3:
        EXPECT_EQ(next, "some-algorithmNameThatIsNotInAll");
        break;
      case 4:
        EXPECT_EQ(next, "with spaces");
        break;
      default:
        throw exception("Unexpected number of algorithm names");
    }

    ++algorithmPos;
  }

  EXPECT_EQ(algorithmPos, 5);
}
}  // namespace cct