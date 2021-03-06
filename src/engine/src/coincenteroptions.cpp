#include "coincenteroptions.hpp"

#include <iostream>

#include "cct_const.hpp"

namespace cct {

void CoincenterCmdLineOptions::PrintVersion(std::string_view programName) {
  std::cout << programName << " version " << CCT_VERSION << std::endl;
  std::cout << "compiled with " << CCT_COMPILER_VERSION << " on " << __DATE__ << " at " << __TIME__ << std::endl;
}

}  // namespace cct
