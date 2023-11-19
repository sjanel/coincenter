#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommands.hpp"
#include "coincenteroptions.hpp"
#include "coincenteroptionsdef.hpp"
#include "commandlineoptionsparser.hpp"
#include "parseoptions.hpp"
#include "processcommandsfromcli.hpp"
#include "runmodes.hpp"

int main(int argc, const char* argv[]) {
  try {
    using namespace cct;
    auto parser =
        CommandLineOptionsParser<CoincenterCmdLineOptions>(CoincenterAllowedOptions<CoincenterCmdLineOptions>::value);
    const auto cmdLineOptionsVector = ParseOptions(parser, argc, argv);

    if (!cmdLineOptionsVector.empty()) {
      const CoincenterCommands coincenterCommands(cmdLineOptionsVector);
      const auto programName = std::filesystem::path(argv[0]).filename().string();

      ProcessCommandsFromCLI(programName, coincenterCommands, cmdLineOptionsVector.front(), settings::RunMode::kProd);
    }
  } catch (const cct::invalid_argument& e) {
    std::cerr << "Invalid argument: " << e.what() << '\n';
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
