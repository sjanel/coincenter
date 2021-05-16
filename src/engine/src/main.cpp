
#include <stdlib.h>

#include "coincenter.hpp"
#include "coincenterparsedoptions.hpp"

int main(int argc, const char* argv[]) {
  try {
    cct::CoincenterParsedOptions opts(argc, argv);

    if (opts.noProcess) {
      return EXIT_SUCCESS;
    }

    cct::Coincenter coincenter;
    coincenter.process(opts);
  } catch (...) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
