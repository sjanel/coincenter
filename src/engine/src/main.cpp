
#include <stdlib.h>

#include "coincenter.hpp"
#include "coincenterparsedoptions.hpp"

int main(int argc, const char* argv[]) {
  try {
    cct::CoincenterParsedOptions opts(argc, argv);

    if (opts.noProcess) {
      return EXIT_SUCCESS;
    }

    cct::MonitoringInfo monitoringInfo(opts.programName(), opts.monitoring_address, opts.monitoring_port,
                                       opts.monitoring_username, opts.monitoring_password);

    cct::Coincenter coincenter(opts.noSecretsExchanges, opts.noSecretsForAll, cct::settings::RunMode::kProd,
                               opts.dataDir, std::move(monitoringInfo));

    coincenter.process(opts);

  } catch (...) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
