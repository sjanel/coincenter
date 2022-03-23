#include <stdlib.h>

#include <filesystem>
#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincentercommands.hpp"
#include "processcommandsfromcli.hpp"

int main(int argc, const char* argv[]) {
  try {
    cct::CoincenterCommands coincenterCommands;

    auto parsedOptions = coincenterCommands.parseOptions(argc, argv);

    if (coincenterCommands.setFromOptions(parsedOptions)) {
      auto programName = std::filesystem::path(argv[0]).filename().string();

      cct::ProcessCommandsFromCLI(programName, coincenterCommands, parsedOptions);
    }
  } catch (const cct::invalid_argument& e) {
    cct::log::critical("Invalid argument: {}", e.what());
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    cct::log::critical("Exception: {}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
