#include "processcommandsfromcli.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace {
constexpr settings::RunMode kRunMode = settings::RunMode::kTestKeys;
constexpr std::string_view kProgramName = "coincenter";
}  // namespace

TEST(ProcessCommandsFromCLI, TestNoArguments) {
  CoincenterCmdLineOptions cmdLineOptions;
  CoincenterCommands coincenterCommands{cmdLineOptions};

  EXPECT_NO_THROW(ProcessCommandsFromCLI(kProgramName, coincenterCommands, cmdLineOptions, kRunMode));
}

TEST(ProcessCommandsFromCLI, TestIncorrectArgument) {
  CoincenterCmdLineOptions cmdLineOptions;
  cmdLineOptions.apiOutputType = "invalid";
  CoincenterCommands coincenterCommands{cmdLineOptions};

  EXPECT_THROW(ProcessCommandsFromCLI(kProgramName, coincenterCommands, cmdLineOptions, kRunMode), invalid_argument);
}

}  // namespace cct