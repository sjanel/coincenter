#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "cct_vector.hpp"
#include "commandlineoption.hpp"
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
  CommandLineOptionalInt32 optInt;
  Duration timeOpt;
};

using ParserType = CommandLineOptionsParser<Opts>;
using CommandLineOptionType = AllowedCommandLineOptionsBase<Opts>::CommandLineOptionType;
using CommandLineOptionWithValue = AllowedCommandLineOptionsBase<Opts>::CommandLineOptionWithValue;

template <class OptValueType>
struct MainOptions {
  static constexpr typename AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue value[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &OptValueType::stringOpt},
      {{{"General", 1},
        "--opt2",
        "",
        "This is a longer description of the option so that there should be at least a new line in output display of "
        "the help."},
       &OptValueType::intOpt},
      {{{"Other", 2},
        "--opt3",
        "",
        "It is a long established fact that a reader will be distracted by the readable content of a page when looking "
        "at its layout. The point of using Lorem Ipsum is that it has a more-or-less normal distribution of letters, "
        "as opposed to using 'Content here, content here', making it look like readable English. Many desktop "
        "publishing packages and web page editors now use Lorem Ipsum as their default model text, and a search for "
        "'lorem ipsum' will uncover many web sites still in their infancy. Various versions have evolved over the "
        "years, sometimes by accident, sometimes on purpose (injected humour and the like)."},
       &OptValueType::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &OptValueType::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &OptValueType::timeOpt},
      {{{"Monitoring", 3}, "--optInt", 'i', "", "Optional int"}, &OptValueType::optInt},
      {{{"Monitoring", 3},
        "--optSV1",
        'v',
        "",
        "There are several strategies that can be used to avoid service disruption while releasing new features for "
        "customers:\n"
        "Gradual rollout: Instead of releasing all of the new features at once, they can be rolled out gradually to a "
        "small percentage of users initially and then gradually increased to a larger percentage over time. This "
        "allows the company to monitor and address any issues that may arise before they affect a larger portion of "
        "users.\n"
        "A/B testing: A/B testing allows you to test new features on a small subset of users before rolling them out "
        "to the entire user base. This allows you to compare the performance of the new feature against the current "
        "version and make adjustments as needed.\n"
        "Canary releases: Canary releases involve releasing new features to a small subset of servers or users before "
        "releasing them to the entire system. This allows you to test the new feature in a production environment and "
        "address any issues before they affect the entire user base.\n"
        "Feature flags: Feature flags allow you to enable or disable specific features for certain users, and this "
        "allows you to test new features before releasing them to the entire user base.\n"
        "Automated testing: Automated testing can help ensure that new features do not cause issues with existing "
        "functionality. This includes unit, integration and end-to-end testing, and can be run before and after "
        "releasing new features.\n"
        "Rollback capability: having a rollback mechanism in place will allow you to quickly roll back to the previous "
        "version of the application if any issues arise with the new release.\n"
        "By using a combination of these strategies, GLaDOS Inc can minimize the risk of service disruption while "
        "still being able to release new features for customers.\n"
        "It's important to keep in mind that testing and monitoring are crucial to a successful release and that a "
        "dedicated team should be in place to ensure that the release goes smoothly and that any issues are quickly "
        "identified and resolved."},
       &OptValueType::sv},
      {{{"Monitoring", 3}, "--optSV2", "", "Optional SV"}, &OptValueType::optSV},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &OptValueType::boolOpt}};
};

class CommandLineOptionsParserTest : public ::testing::Test {
 public:
  Opts createOptions(std::initializer_list<const char *> init) {
    vector<const char *> opts(init.begin(), init.end());
    return _parser.parse(opts);
  }

 protected:
  ParserType _parser{MainOptions<Opts>::value};
};

TEST_F(CommandLineOptionsParserTest, Basic) {
  Opts options = createOptions({"--opt1", "toto", "--help"});
  EXPECT_EQ(options.stringOpt, "toto");
  EXPECT_TRUE(options.boolOpt);

  EXPECT_THROW(createOptions({"--opt1", "toto", "--opt3", "--opt2"}), invalid_argument);
  EXPECT_THROW(createOptions({"--opt1", "toto", "--opts3", "--opt2", "3"}), invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, StringView1) {
  EXPECT_EQ(createOptions({"--optSV1", "Hey Listen!"}).sv, std::string_view("Hey Listen!"));
}

TEST_F(CommandLineOptionsParserTest, StringView2) {
  EXPECT_EQ(createOptions({"--optSV1", ""}).sv, std::string_view(""));
}

TEST_F(CommandLineOptionsParserTest, OptStringViewEmpty) {
  EXPECT_EQ(createOptions({"--optSV2", "--help"}).optSV, std::string_view(""));
}

TEST_F(CommandLineOptionsParserTest, OptStringViewNotEmpty) {
  EXPECT_EQ(createOptions({"--optSV2", "I need to save the world"}).optSV,
            std::optional<std::string_view>("I need to save the world"));
}

TEST_F(CommandLineOptionsParserTest, AlternativeOptionName) {
  EXPECT_TRUE(createOptions({"-h"}).boolOpt);
  EXPECT_THROW(createOptions({"-j"}), invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, String) {
  EXPECT_EQ(createOptions({"--opt1", "2000 EUR, kraken"}).stringOpt, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, OptStringNotEmpty) {
  EXPECT_EQ(createOptions({"--opt4", "2000 EUR, kraken"}).optStr, std::optional<std::string_view>("2000 EUR, kraken"));
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty1) {
  EXPECT_EQ(createOptions({"--opt4", "--opt1", "Opt1 value"}).optStr,
            std::optional<std::string_view>(std::string_view{}));
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty2) {
  EXPECT_EQ(createOptions({"--opt4"}).optStr, std::optional<std::string_view>(std::string_view{}));
}

TEST_F(CommandLineOptionsParserTest, OptStringEmpty3) { EXPECT_EQ(createOptions({"--help"}).optStr, std::nullopt); }

TEST_F(CommandLineOptionsParserTest, OptIntNotEmpty) {
  CommandLineOptionalInt32 optInt = createOptions({"--optInt", "-42", "--opt4", "2000 EUR, kraken"}).optInt;
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_TRUE(optInt.isSet());
  EXPECT_EQ(*optInt, -42);
}

TEST_F(CommandLineOptionsParserTest, OptIntPresent) {
  CommandLineOptionalInt32 optInt = createOptions({"--optInt", "--opt1", "Opt1 value"}).optInt;
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_FALSE(optInt.isSet());
}

TEST_F(CommandLineOptionsParserTest, OptIntPresent2) {
  CommandLineOptionalInt32 optInt = createOptions({"--opt1", "Opt1 value", "--optInt"}).optInt;
  EXPECT_TRUE(optInt.isPresent());
  EXPECT_FALSE(optInt.isSet());
}

TEST_F(CommandLineOptionsParserTest, OptIntUnset) {
  CommandLineOptionalInt32 optInt = createOptions({"--opt1", "Opt1 value"}).optInt;
  EXPECT_FALSE(optInt.isPresent());
}

TEST_F(CommandLineOptionsParserTest, DurationOptionMinutesSpace) {
  EXPECT_EQ(createOptions({"--opt5", "1h45 min"}).timeOpt, std::chrono::hours(1) + std::chrono::minutes(45));
}

TEST_F(CommandLineOptionsParserTest, DurationOptionThrowInvalidTimeUnit1) {
  EXPECT_THROW(createOptions({"--opt5", "13z"}), invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, DisplayHelp) {
  std::ostringstream ostream;
  _parser.displayHelp("programName", ostream);
  constexpr std::string_view expected = R"(usage: programName <general options> [command(s)]
Options:

 General
  --help, -h           Help descr
  --opt1, -o <myValue> Opt1 descr
  --opt2               This is a longer description of the option so that there should be at least a new line in output 
                       display of the help.

 Other
  --opt3               It is a long established fact that a reader will be distracted by the readable content of a page 
                       when looking at its layout. The point of using Lorem Ipsum is that it has a more-or-less normal 
                       distribution of letters, as opposed to using 'Content here, content here', making it look like 
                       readable English. Many desktop publishing packages and web page editors now use Lorem Ipsum as 
                       their default model text, and a search for 'lorem ipsum' will uncover many web sites still in 
                       their infancy. Various versions have evolved over the years, sometimes by accident, sometimes on 
                       purpose (injected humour and the like).
  --opt4               Opt4 descr
  --opt5               Opt5 time unit

 Monitoring
  --optInt, -i         Optional int
  --optSV1, -v         There are several strategies that can be used to avoid service disruption while releasing new 
                       features for customers:
                       Gradual rollout: Instead of releasing all of the new features at once, they can be rolled out 
                       gradually to a small percentage of users initially and then gradually increased to a larger 
                       percentage over time. This allows the company to monitor and address any issues that may arise 
                       before they affect a larger portion of users.
                       A/B testing: A/B testing allows you to test new features on a small subset of users before 
                       rolling them out to the entire user base. This allows you to compare the performance of the new 
                       feature against the current version and make adjustments as needed.
                       Canary releases: Canary releases involve releasing new features to a small subset of servers or 
                       users before releasing them to the entire system. This allows you to test the new feature in a 
                       production environment and address any issues before they affect the entire user base.
                       Feature flags: Feature flags allow you to enable or disable specific features for certain users, 
                       and this allows you to test new features before releasing them to the entire user base.
                       Automated testing: Automated testing can help ensure that new features do not cause issues with 
                       existing functionality. This includes unit, integration and end-to-end testing, and can be run 
                       before and after releasing new features.
                       Rollback capability: having a rollback mechanism in place will allow you to quickly roll back to 
                       the previous version of the application if any issues arise with the new release.
                       By using a combination of these strategies, GLaDOS Inc can minimize the risk of service 
                       disruption while still being able to release new features for customers.
                       It's important to keep in mind that testing and monitoring are crucial to a successful release 
                       and that a dedicated team should be in place to ensure that the release goes smoothly and that 
                       any issues are quickly identified and resolved.
  --optSV2             Optional SV
)";
  EXPECT_EQ(ostream.view(), expected);
}

struct OptsExt : public Opts {
  int int3Opt = 0;
  std::string_view sv2;
};

using ExtParserType = CommandLineOptionsParser<OptsExt>;
using ExtCommandLineOptionType = AllowedCommandLineOptionsBase<OptsExt>::CommandLineOptionType;
using ExtCommandLineOptionWithValue = AllowedCommandLineOptionsBase<OptsExt>::CommandLineOptionWithValue;

static constexpr std::array kAdditionalOpts = {
    ExtCommandLineOptionWithValue{{{"Monitoring", 3}, "--optExt", "", "extension value string"}, &OptsExt::sv2},
    ExtCommandLineOptionWithValue{{{"Monitoring", 3}, "--intExt", "", "extension value int"}, &OptsExt::int3Opt},
};

class CommandLineOptionsParserExtTest : public ::testing::Test {
 public:
  CommandLineOptionsParserExtTest() { _parser.append(kAdditionalOpts); }

  OptsExt createOptions(std::initializer_list<const char *> init) {
    vector<const char *> opts(init.begin(), init.end());
    return _parser.parse(opts);
  }

 protected:
  ExtParserType _parser{MainOptions<OptsExt>::value};
};

TEST_F(CommandLineOptionsParserExtTest, AppendOtherOptions) {
  static_assert(StaticCommandLineOptionsDuplicatesCheck(std::to_array(MainOptions<OptsExt>::value), kAdditionalOpts),
                "It should detect no duplicated option names");
  EXPECT_EQ(createOptions({"--optSV1", "Hey Listen!"}).sv, std::string_view("Hey Listen!"));
  EXPECT_EQ(createOptions({"--optExt", "I am your father"}).sv2, std::string_view("I am your father"));
  EXPECT_NE(createOptions({"--optExt", "I am your father"}).sv, std::string_view("Hey Listen!"));
}

TEST(CommandLineOptionsParserDuplicatesTest, StaticDuplicateCheckOnShortName) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", 'o', "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsDuplicatesCheck(std::to_array(options)),
                "It should detect duplicated options by short name o");
}

TEST(CommandLineOptionsParserDuplicatesTest, StaticDuplicateCheckOnLongName) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsDuplicatesCheck(std::to_array(options)),
                "It should detect duplicated options by long name opt2");
  static_assert(StaticCommandLineOptionsDescriptionCheck(std::to_array(options)), "No option with bad description");
}

TEST(CommandLineOptionsParserDuplicatesTest, StaticDuplicateCheckOnLongNameCombined) {
  constexpr CommandLineOptionWithValue options1[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  constexpr CommandLineOptionWithValue options2[] = {
      {{{"General", 1}, "--opt3", "<myValue>", "Opt1 descr\n"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt4", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "help", "", "Help descr"}, &Opts::boolOpt}};
  static_assert(!StaticCommandLineOptionsDuplicatesCheck(std::to_array(options1), std::to_array(options2)),
                "It should detect duplicated options by long name help");
  static_assert(!StaticCommandLineOptionsDescriptionCheck(std::to_array(options2)), "One option with bad description");
}

TEST(CommandLineOptionsParserDuplicatesTest, StaticDuplicateCheckOKCombined) {
  constexpr CommandLineOptionWithValue options1[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt2", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  constexpr CommandLineOptionWithValue options2[] = {
      {{{"General", 1}, "--opt3", "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"Other", 2}, "--opt4", "", "Opt5 time unit"}, &Opts::timeOpt}};
  static_assert(StaticCommandLineOptionsDuplicatesCheck(std::to_array(options1), std::to_array(options2)),
                "It should detect no duplicated options");
}

TEST(CommandLineOptionsParserDuplicatesTest, StaticDuplicateCheckOK) {
  constexpr CommandLineOptionWithValue options[] = {
      {{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
      {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
      {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
      {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
      {{{"Other", 2}, "--opt5", "", "Opt5 time unit"}, &Opts::timeOpt},
      {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}};
  static_assert(StaticCommandLineOptionsDuplicatesCheck(std::to_array(options)),
                "It should not detect duplicated options");
}

TEST(CommandLineOptionsParserDuplicatesTest, NoDuplicateCheckAtRuntime) {
  EXPECT_NO_THROW(ParserType({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                              {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                              {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                              {{{"Other", 2}, "--opt4", "", "Opt4 descr"}, &Opts::optStr},
                              {{{"Other", 2}, "--opt2", "", "Opt2 time unit"}, &Opts::timeOpt},
                              {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}));
}

}  // namespace cct