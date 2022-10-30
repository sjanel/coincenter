#include "cct_exception.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CctExceptionTest, InfoTakenFromConstCharStar) {
  exception ex("This string can fill the inline storage");
  EXPECT_STREQ(ex.what(), "This string can fill the inline storage");
}

TEST(CctExceptionTest, MoveStringConstructorEmpty) {
  string str;
  exception ex(std::move(str));
  EXPECT_STREQ(ex.what(), "");
}

TEST(CctExceptionTest, MoveStringConstructorLong) {
  string str(
      "This String is a big string. In fact I make this string to be more than 128 chars. That way, "
      "I can test if the exception stores it correctly.");
  exception ex(std::move(str));
  EXPECT_STREQ(ex.what(),
               "This String is a big string. In fact I make this string to be more than 128 chars. That way, "
               "I can test if the exception stores it correctly.");
}

#ifndef CCT_MSVC
TEST(CctExceptionTest, Format) {
  EXPECT_STREQ(exception("Unknown currency code {}. Please enter a real one.", "BABYDOGE").what(),
               "Unknown currency code BABYDOGE. Please enter a real one.");
}
#endif

}  // namespace cct