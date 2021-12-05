#include <stdlib.h>

#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "coincenterparsedoptions.hpp"

int main(int argc, const char* argv[]) {
  try {
    cct::CoincenterParsedOptions opts(argc, argv);

    if (opts.noProcess) {
      return EXIT_SUCCESS;
    }

    cct::MonitoringInfo monitoringInfo(opts.useMonitoring, opts.programName(), opts.monitoring_address,
                                       opts.monitoring_port, opts.monitoring_username, opts.monitoring_password);

    cct::CoincenterInfo coincenterInfo(cct::settings::RunMode::kProd, opts.dataDir, std::move(monitoringInfo),
                                       opts.printQueryResults);

    cct::Coincenter coincenter(coincenterInfo, opts.exchangesSecretsInfo);

    coincenter.process(opts);

    coincenter.updateFileCaches();  // Write potentially updated cache data on disk at end of program

  } catch (...) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
