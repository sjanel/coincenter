#include <stdlib.h>

#include <filesystem>
#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "coincentercommands.hpp"

int main(int argc, const char* argv[]) {
  try {
    auto programName = std::filesystem::path(argv[0]).filename().string();
    cct::CoincenterCommands opts;

    auto parsedOptions = opts.parseOptions(argc, argv);

    if (opts.setFromOptions(parsedOptions)) {
      cct::LoadConfiguration loadConfiguration(parsedOptions.dataDir,
                                               cct::LoadConfiguration::ExchangeConfigFileType::kProd);
      cct::CoincenterInfo coincenterInfo(cct::settings::RunMode::kProd, loadConfiguration,
                                         opts.createMonitoringInfo(programName, parsedOptions), opts.printQueryResults);

      cct::Coincenter coincenter(coincenterInfo, opts.exchangesSecretsInfo);

      coincenter.process(opts);

      coincenter.updateFileCaches();  // Write potentially updated cache data on disk at end of program
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
