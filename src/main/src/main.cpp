#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommands.hpp"
#include "processcommandsfromcli.hpp"
#include "runmodes.hpp"

int main(int argc, const char* argv[]) {
  try {
    auto cmdLineOptionsVector = cct::CoincenterCommands::ParseOptions(argc, argv);

    if (!cmdLineOptionsVector.empty()) {
      cct::CoincenterCommands coincenterCommands(cmdLineOptionsVector);
      auto programName = std::filesystem::path(argv[0]).filename().string();

      cct::ProcessCommandsFromCLI(programName, coincenterCommands, cmdLineOptionsVector.front(),
                                  cct::settings::RunMode::kProd);
    }
  } catch (const cct::invalid_argument& e) {
    std::cerr << "Invalid argument: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
