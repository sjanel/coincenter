#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincentercommands.hpp"
#include "processcommandsfromcli.hpp"

int main(int argc, const char* argv[]) {
  try {
    auto cmdLineOptions = cct::CoincenterCommands::ParseOptions(argc, argv);

    if (!cmdLineOptions.help && !cmdLineOptions.version) {
      cct::CoincenterCommands coincenterCommands(cmdLineOptions);
      auto programName = std::filesystem::path(argv[0]).filename().string();

      cct::ProcessCommandsFromCLI(programName, coincenterCommands, cmdLineOptions, cct::settings::RunMode::kProd);
    }
  } catch (const cct::invalid_argument& e) {
    cct::log::critical("Invalid argument: {}", e.what());
    return EXIT_FAILURE;
  } catch ([[maybe_unused]] const std::exception& e) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
