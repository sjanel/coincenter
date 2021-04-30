
#include "commandlineoptionsparser.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(CommandLineOptionsParser, Basic) {
  struct Opts {
    std::string stringOpt{};
    int intOpt{};
    bool boolOpt{};
  };

  CmdOpts parser =
      CmdOpts<Opts>::Create({{"--opt1", &Opts::stringOpt}, {"--opt2", &Opts::intOpt}, {"--opt3", &Opts::boolOpt}});

  Opts options = parser.parse({"coincenter", "--opt1", "toto", "--opt3"});
  EXPECT_EQ(options.stringOpt, "toto");
  EXPECT_TRUE(options.boolOpt);

  EXPECT_THROW(parser.parse({"coincenter", "--opt1", "toto", "--opt3", "--opt2"}), std::invalid_argument);
  EXPECT_THROW(parser.parse({"coincenter", "--opt1", "toto", "--opts3", "--opt2", "3"}), std::invalid_argument);
}

TEST(CommandLineOptionsParser, String) {
  struct Opts {
    std::string stringOpt{};
  };

  CmdOpts parser = CmdOpts<Opts>::Create({{"--opt1", &Opts::stringOpt}});

  Opts options = parser.parse({"coincenter", "--opt1", "2000 EUR, kraken"});
  EXPECT_EQ(options.stringOpt, "2000 EUR, kraken");
}
}  // namespace cct