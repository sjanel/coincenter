
#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

#include "cct_vector.hpp"

namespace cct {

class CommandLineOptionsParserTest : public ::testing::Test {
 public:
  struct Opts {
    std::string stringOpt{};
    int intOpt{};
    int int2Opt{};
    bool boolOpt{};
  };

  using ParserType = CommandLineOptionsParser<Opts>;

  CommandLineOptionsParserTest()
      : _parser({{{{"General", 1}, "--opt1", 'o', "<myValue>", "Opt1 descr"}, &Opts::stringOpt},
                 {{{"General", 1}, "--opt2", "", "Opt2 descr"}, &Opts::intOpt},
                 {{{"Other", 2}, "--opt3", "", "Opt3 descr"}, &Opts::int2Opt},
                 {{{"General", 1}, "--help", 'h', "", "Help descr"}, &Opts::boolOpt}}) {}

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  ParserType _parser;
};

TEST_F(CommandLineOptionsParserTest, Basic) {
  cct::vector<const char *> opts1 = {"coincenter", "--opt1", "toto", "--help"};
  Opts options = _parser.parse(opts1);
  EXPECT_EQ(options.stringOpt, "toto");
  EXPECT_TRUE(options.boolOpt);

  cct::vector<const char *> opts2 = {"coincenter", "--opt1", "toto", "--opt3", "--opt2"};
  EXPECT_THROW(_parser.parse(opts2), std::invalid_argument);
  cct::vector<const char *> opts3 = {"coincenter", "--opt1", "toto", "--opts3", "--opt2", "3"};
  EXPECT_THROW(_parser.parse(opts3), std::invalid_argument);
}

TEST_F(CommandLineOptionsParserTest, String) {
  cct::vector<const char *> opts = {"coincenter", "--opt1", "2000 EUR, kraken"};
  Opts options = _parser.parse(opts);
  EXPECT_EQ(options.stringOpt, "2000 EUR, kraken");
}

TEST_F(CommandLineOptionsParserTest, AlternativeOptionName) {
  cct::vector<const char *> opts = {"coincenter", "-h"};
  Opts options = _parser.parse(opts);
  EXPECT_TRUE(options.boolOpt);
  cct::vector<const char *> opts2 = {"coincenter", "-j"};
  EXPECT_THROW(_parser.parse(opts2), std::invalid_argument);
}

}  // namespace cct