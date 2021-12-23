#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

#include "cct_vector.hpp"
#include "timehelpers.hpp"

namespace cct {

struct Opts {
  string stringOpt;
  int intOpt = 0;
  int int2Opt = 0;
  bool boolOpt = false;
  std::optional<string> optStr;
  CommandLineOptionalInt optInt;
  Duration timeOpt;
};

using ParserType = CommandLineOptionsParser<Opts>;

class CommandLineOptionsParserTest : public ::testing::Test {
 public:
  CommandLineOptionsParserTest()
      : _parser({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                 {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                 {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                 {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                 {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
                 {{{"Monitoring", 3}, "--optInt", 'i', "", "Optional int"}, &Opts::optInt},
                 {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}) {}

  Opts createOptions(std::initializer_list<const char *> init) {
    vector<const char *> opts(init.begin(), init.end());
    return _parser.parse(opts);
  }

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  ParserType _parser;
};

TEST_F(CommandLineOptionsParserTest, Basic) {
  Opts options = createOptions({"coincenter", "--opt1", "toto", "--help"});
  EXPECT_EQ(options.stringOpt, "toto");
  EXPECT_TRUE(options.boolOpt);

  EXPECT_THROW(createOptions({"coincenter", "--opt1", "toto", "--opt3", "--opt2"}), std::invalid_argument);
  EXPECT_THROW(createOptions({"coincenter", "--opt1", "toto", "--opts3", "--opt2", "3"}), std::invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, String) {
  EXPECT_EQ(createOptions({"coincenter", "--opt1", "2000 EUR, kraken"}).stringOpt, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, AlternativeOptionName) {
  EXPECT_TRUE(createOptions({"coincenter", "-h"}).boolOpt);
  EXPECT_THROW(createOptions({"coincenter", "-j"}), std::invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, OptStringNotEmpty) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4", "2000 EUR, kraken"}).optStr, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty1) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4", "--opt1", "Opt1 value"}).optStr, string());
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty2) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4"}).optStr, string());
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty3) {
  EXPECT_EQ(createOptions({"coincenter", "--help"}).optStr, std::nullopt);
}

TEST_F(CommandLineOptionsParserTest, OptIntNotEmpty) {
  CommandLineOptionalInt optInt = createOptions({"coincenter", "--optInt", "-42", "--opt4", "2000 EUR, kraken"}).optInt;
  EXPECT_EQ(*optInt, -42);
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_TRUE(optInt.isSet());
}

TEST_F(CommandLineOptionsParserTest, OptIntPresent) {
  CommandLineOptionalInt optInt = createOptions({"coincenter", "--optInt", "--opt1", "Opt1 value"}).optInt;
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_FALSE(optInt.isSet());
}

TEST_F(CommandLineOptionsParserTest, OptIntPresent2) {
  CommandLineOptionalInt optInt = createOptions({"coincenter", "--opt1", "Opt1 value", "--optInt"}).optInt;
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_FALSE(optInt.isSet());
}

TEST_F(CommandLineOptionsParserTest, OptIntUnset) {
  CommandLineOptionalInt optInt = createOptions({"coincenter", "--opt1", "Opt1 value"}).optInt;
  EXPECT_FALSE(optInt.isPresent());
}

TEST_F(CommandLineOptionsParserTest, DurationOptionDays) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "37d"}).timeOpt, std::chrono::days(37));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionHours) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "12h"}).timeOpt, std::chrono::hours(12));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionMinutesSpace) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "1h45 min"}).timeOpt,
            std::chrono::hours(1) + std::chrono::minutes(45));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionSeconds) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "3s"}).timeOpt, std::chrono::seconds(3));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionMilliseconds) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "1500 ms"}).timeOpt, std::chrono::milliseconds(1500));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionMicroseconds) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "567889358us"}).timeOpt, std::chrono::microseconds(567889358));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionNanoseconds) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "100000000000000  ns"}).timeOpt,
            std::chrono::nanoseconds(100000000000000L));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionLongTime) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "3y9mon2w5min"}).timeOpt,
            std::chrono::years(3) + std::chrono::months(9) + std::chrono::weeks(2) + std::chrono::minutes(5));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowInvalidTimeUnit1) {
  EXPECT_THROW(createOptions({"coincenter", "--opt5", "13z"}), InvalidArgumentException);
}

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowInvalidTimeUnit2) {
  EXPECT_THROW(createOptions({"coincenter", "--opt5", "42"}), InvalidArgumentException);
}

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowOnlyIntegral) {
  EXPECT_THROW(createOptions({"coincenter", "--opt5", "2.5min"}), InvalidArgumentException);
}

TEST(CommandLineOptionsParserTestDuplicates, DuplicateCheckOnShortNameAtInit) {
  EXPECT_THROW(ParserType({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                           {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                           {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                           {{{"Other", 2}, "--opt4", 'o', "", "Opt4 descr"}, &Opts::optStr},
                           {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
                           {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}),
               InvalidArgumentException);
}

TEST(CommandLineOptionsParserTestDuplicates, DuplicateCheckOnShortNameAtInsert) {
  ParserType parser({{{{"General", 1}, "--opt1", "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                     {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                     {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                     {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                     {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
                     {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}});
  EXPECT_THROW(parser.insert({{{"General", 1}, "--opt1", 'h', "<myValue>", "Opt1 descr"}, &Opts::stringOpt}),
               InvalidArgumentException);
}

TEST(CommandLineOptionsParserTestDuplicates, DuplicateCheckOnLongNameAtInit) {
  EXPECT_THROW(ParserType({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                           {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                           {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                           {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                           {{{"Other", 2}, "--opt2", "", "Opt2 time unit"}, &Opts::timeOpt},
                           {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}),
               InvalidArgumentException);
}

TEST(CommandLineOptionsParserTestDuplicates, DuplicateCheckOnLongNameAtInsert) {
  ParserType parser({{{{"General", 1}, "--opt1", "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                     {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                     {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                     {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                     {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
                     {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}});
  EXPECT_THROW(parser.insert({{{"General", 1}, "--opt3", "<myValue>", "Opt1 descr"}, &Opts::stringOpt}),
               InvalidArgumentException);
}

}  // namespace cct