#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

#include "cct_vector.hpp"

namespace cct {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = Clock::duration;

struct Opts {
  string stringOpt{};
  int intOpt{};
  int int2Opt{};
  bool boolOpt{};
  std::optional<string> optStr{};
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
                 {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}) {}

  Opts createOptions(std::initializer_list<const char *> init) {
    vector<const char *> opts = init;
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

TEST_F(CommandLineOptionsParserTest, DurationOptionHours) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "12h"}).timeOpt, std::chrono::hours(12));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionMinutesSpace) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "45 min"}).timeOpt, std::chrono::minutes(45));
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

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowInvalidTimeUnit1) {
  EXPECT_THROW(createOptions({"coincenter", "--opt5", "13mon"}), InvalidArgumentException);
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