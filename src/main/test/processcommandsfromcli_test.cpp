#include "processcommandsfromcli.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommands.hpp"
#include "coincenteroptions.hpp"
#include "runmodes.hpp"

namespace cct {
namespace {
constexpr settings::RunMode kRunMode = settings::RunMode::kTestKeys;
constexpr std::string_view kProgramName = "coincenter";
}  // namespace

TEST(ProcessCommandsFromCLI, TestNoArguments) {
  CoincenterCmdLineOptions cmdLineOptions;
  CoincenterCommands coincenterCommands{std::span<const CoincenterCmdLineOptions>(&cmdLineOptions, 1U)};

  EXPECT_NO_THROW(ProcessCommandsFromCLI(kProgramName, coincenterCommands, cmdLineOptions, kRunMode));
}

TEST(ProcessCommandsFromCLI, TestIncorrectArgument) {
  CoincenterCmdLineOptions cmdLineOptions;
  cmdLineOptions.apiOutputType = "invalid";
  CoincenterCommands coincenterCommands{std::span<const CoincenterCmdLineOptions>(&cmdLineOptions, 1U)};

  EXPECT_THROW(ProcessCommandsFromCLI(kProgramName, coincenterCommands, cmdLineOptions, kRunMode), invalid_argument);
}

}  // namespace cct