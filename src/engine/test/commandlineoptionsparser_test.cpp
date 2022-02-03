#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

#include "cct_invalid_argument_exception.hpp"
#include "cct_vector.hpp"
#include "staticcommandlineoptioncheck.hpp"
#include "timedef.hpp"

namespace cct {

struct Opts {
  std::string_view stringOpt;
  std::optional<std::string_view> optStr;
  std::string_view sv;
  std::optional<std::string_view> optSV;
  int intOpt = 0;
  int int2Opt = 0;
  bool boolOpt = false;
  CommandLineOptionalInt optInt;
  Duration timeOpt;
};

using ParserType = CommandLineOptionsParser<Opts>;
using CommandLineOptionType = AllowedCommandLineOptionsBase<Opts>::CommandLineOptionType;
using CommandLineOptionWithValue = AllowedCommandLineOptionsBase<Opts>::CommandLineOptionWithValue;

template <class OptValueType>
struct MainOptions {
  static constexpr typename AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue value[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &OptValueType::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &OptValueType::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &OptValueType::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &OptValueType::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &OptValueType::timeOpt},
      {{{"Monitoring", 3}, "--optInt", 'i', "", "Optional int"}, &OptValueType::optInt},
      {{{"Monitoring", 3}, "--optSV1", 'v', "", "Username"}, &OptValueType::sv},
      {{{"Monitoring", 3}, "--optSV2", "", "Optional SV"}, &OptValueType::optSV},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &OptValueType::boolOpt}};
};

class CommandLineOptionsParserTest : public ::testing::Test {
 public:
  CommandLineOptionsParserTest() : _parser(MainOptions<Opts>::value) {}

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

  EXPECT_THROW(createOptions({"coincenter", "--opt1", "toto", "--opt3", "--opt2"}), invalid_argument);
  EXPECT_THROW(createOptions({"coincenter", "--opt1", "toto", "--opts3", "--opt2", "3"}), invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, StringView1) {
  EXPECT_EQ(createOptions({"coincenter", "--optSV1", "Hey Listen!"}).sv, std::string_view("Hey Listen!"));
}

TEST_F(CommandLineOptionsParserTest, StringView2) {
  EXPECT_EQ(createOptions({"coincenter", "--optSV1", ""}).sv, std::string_view(""));
}

TEST_F(CommandLineOptionsParserTest, OptStringViewEmpty) {
  EXPECT_EQ(createOptions({"coincenter", "--optSV2", "--help"}).optSV, std::string_view(""));
}

TEST_F(CommandLineOptionsParserTest, OptStringViewNotEmpty) {
  EXPECT_EQ(*createOptions({"coincenter", "--optSV2", "I need to save the world"}).optSV,
            std::string_view("I need to save the world"));
}

TEST_F(CommandLineOptionsParserTest, AlternativeOptionName) {
  EXPECT_TRUE(createOptions({"coincenter", "-h"}).boolOpt);
  EXPECT_THROW(createOptions({"coincenter", "-j"}), invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, String) {
  EXPECT_EQ(createOptions({"coincenter", "--opt1", "2000 EUR, kraken"}).stringOpt, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, OptStringNotEmpty) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4", "2000 EUR, kraken"}).optStr, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty1) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4", "--opt1", "Opt1 value"}).optStr, std::string_view());
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty2) {
  EXPECT_EQ(*createOptions({"coincenter", "--opt4"}).optStr, std::string_view());
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

TEST_F(CommandLineOptionsParserTest, DurationOptionMinutesSpace) {
  EXPECT_EQ(createOptions({"coincenter", "--opt5", "1h45 min"}).timeOpt,
            std::chrono::hours(1) + std::chrono::minutes(45));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowInvalidTimeUnit1) {
  EXPECT_THROW(createOptions({"coincenter", "--opt5", "13z"}), invalid_argument);
}

struct OptsExt : public Opts {
  int int3Opt = 0;
  std::string_view sv2;
};

using ExtParserType = CommandLineOptionsParser<OptsExt>;
using ExtCommandLineOptionType = AllowedCommandLineOptionsBase<OptsExt>::CommandLineOptionType;
using ExtCommandLineOptionWithValue = AllowedCommandLineOptionsBase<OptsExt>::CommandLineOptionWithValue;

static constexpr ExtCommandLineOptionWithValue kAdditionalOpts[] = {
    {{{"Monitoring", 3}, "--optExt", "", "extension value string"}, &OptsExt::sv2},
    {{{"Monitoring", 3}, "--intExt", "", "extension value int"}, &OptsExt::int3Opt},
};

class CommandLineOptionsParserExtTest : public ::testing::Test {
 public:
  CommandLineOptionsParserExtTest() : _parser(MainOptions<OptsExt>::value) { _parser.append(kAdditionalOpts); }

  OptsExt createOptions(std::initializer_list<const char *> init) {
    vector<const char *> opts(init.begin(), init.end());
    return _parser.parse(opts);
  }

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  ExtParserType _parser;
};

TEST_F(CommandLineOptionsParserExtTest, AppendOtherOptions) {
  static_assert(
      StaticCommandLineOptionsCheck(std::to_array(MainOptions<OptsExt>::value), std::to_array(kAdditionalOpts)),
      "It should detect no duplicated option names");
  EXPECT_EQ(createOptions({"coincenter", "--optSV1", "Hey Listen!"}).sv, std::string_view("Hey Listen!"));
  EXPECT_EQ(createOptions({"coincenter", "--optExt", "I am your father"}).sv2, std::string_view("I am your father"));
}

TEST(CommandLineOptionsParserTestDuplicates, StaticDuplicateCheckOnShortName) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", 'o', "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsCheck(std::to_array(options)),
                "It should detect duplicated options by short name o");
}

TEST(CommandLineOptionsParserTestDuplicates, StaticDuplicateCheckOnLongName) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "--opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsCheck(std::to_array(options)),
                "It should detect duplicated options by long name --opt2");
}

TEST(CommandLineOptionsParserTestDuplicates, StaticDuplicateCheckOnLongNameCombined) {
  constexpr CommandLineOptionWithValue options1[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  constexpr CommandLineOptionWithValue options2[] = {
      {{{"General", 1}, "--opt3", "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt4", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsCheck(std::to_array(options1), std::to_array(options2)),
                "It should detect duplicated options by long name --help");
}

TEST(CommandLineOptionsParserTestDuplicates, StaticDuplicateCheckOKCombined) {
  constexpr CommandLineOptionWithValue options1[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  constexpr CommandLineOptionWithValue options2[] = {
      {{{"General", 1}, "--opt3", "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt4", "", "Opt5 time unit"}, &Opts::timeOpt}};
  static_assert(StaticCommandLineOptionsCheck(std::to_array(options1), std::to_array(options2)),
                "It should detect no duplicated options");
}

TEST(CommandLineOptionsParserTestDuplicates, StaticDuplicateCheckOK) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(StaticCommandLineOptionsCheck(std::to_array(options)), "It should not detect duplicated options");
}

TEST(CommandLineOptionsParserTestDuplicates, NoDuplicateCheckAtRuntime) {
  EXPECT_NO_THROW(ParserType({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                              {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                              {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                              {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                              {{{"Other", 2}, "--opt2", "", "Opt2 time unit"}, &Opts::timeOpt},
                              {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}));
}

}  // namespace cct