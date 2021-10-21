#include "cct_exception.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace {
const char* const kShortStr = "This string can fill 2 times in the inline storage";
const char* const kVeryLongStr =
    "This String is a big string. In fact I make this string to be more than 128 chars. That way, "
    "I can test if the exception stores it correctly.";
}  // namespace

TEST(CctExceptionTest, EdgeCases) {
  exception ex(nullptr);
  EXPECT_STREQ(ex.what(), "");
}

TEST(CctExceptionTest, InfoTakenFromConstCharStar) {
  exception ex(kShortStr);
  EXPECT_STREQ(ex.what(), kShortStr);

  ex = exception(kVeryLongStr);
  EXPECT_STREQ(ex.what(), kVeryLongStr);
}

TEST(CctExceptionTest, InlineStorage) {
  string shortStrStdString(kShortStr);
  exception ex(shortStrStdString);
  EXPECT_EQ(ex.what(), shortStrStdString);

  shortStrStdString += shortStrStdString;

  ex = exception(shortStrStdString);
  EXPECT_EQ(ex.what(), shortStrStdString);

  string longStrStdString(kVeryLongStr);
  ex = exception(longStrStdString);
  EXPECT_LT(strlen(ex.what()), longStrStdString.length());
  EXPECT_EQ(ex.what(), string(longStrStdString.begin(), longStrStdString.begin() + exception::kMsgMaxLen));

  string longStrStdStringCopy = longStrStdString;
  ex = exception(std::move(longStrStdString));
  EXPECT_EQ(ex.what(), longStrStdStringCopy);
}

TEST(CctExceptionTest, MoveStringConstructor) {
  string str;
  exception ex(std::move(str));
  EXPECT_STREQ(ex.what(), "");
}

TEST(CctExceptionTest, RuleOfFive) {
  string longStr(kVeryLongStr);
  string shortStr(kShortStr);
  exception exWithLongStr(std::move(longStr));
  exception exWithShortStr(std::move(shortStr));

  EXPECT_STREQ(exWithLongStr.what(), kVeryLongStr);
  EXPECT_STREQ(exWithShortStr.what(), kShortStr);
  exWithShortStr = exWithLongStr;
  EXPECT_STRNE(exWithShortStr.what(), kVeryLongStr);
  EXPECT_EQ(string(exWithShortStr.what()), string(kVeryLongStr).substr(0, exception::kMsgMaxLen));
  exWithShortStr = std::move(exWithLongStr);
  EXPECT_STREQ(exWithShortStr.what(), kVeryLongStr);
}

}  // namespace cct