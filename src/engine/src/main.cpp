#include <stdlib.h>

#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "coincenterparsedoptions.hpp"

int main(int argc, const char* argv[]) {
  try {
    cct::CoincenterParsedOptions opts(argc, argv);

    if (opts.noProcess) {
      return EXIT_SUCCESS;
    }

    cct::LoadConfiguration loadConfiguration(opts.dataDir, cct::LoadConfiguration::ExchangeConfigFileType::kProd);
    cct::CoincenterInfo coincenterInfo(cct::settings::RunMode::kProd, loadConfiguration, std::move(opts.monitoringInfo),
                                       opts.printQueryResults);

    cct::Coincenter coincenter(coincenterInfo, opts.exchangesSecretsInfo);

    coincenter.process(opts);

    coincenter.updateFileCaches();  // Write potentially updated cache data on disk at end of program

  } catch (const cct::invalid_argument& e) {
    cct::log::critical("Invalid argument: {}", e.what());
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    cct::log::critical("Exception: {}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
