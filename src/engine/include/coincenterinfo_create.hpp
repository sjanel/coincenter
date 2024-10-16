#pragma once

#include <string_view>

#include "coincenterinfo.hpp"
#include "coincenteroptions.hpp"
#include "exchangesecretsinfo.hpp"
#include "runmodes.hpp"

namespace cct {

CoincenterInfo CoincenterInfo_Create(std::string_view programName, const CoincenterCmdLineOptions &cmdLineOptions,
                                     settings::RunMode runMode);

ExchangeSecretsInfo ExchangeSecretsInfo_Create(const CoincenterCmdLineOptions &cmdLineOptions);

}  // namespace cct