#include "signal-handler.hpp"

#include <csignal>

#include "cct_log.hpp"

namespace {

extern "C" void SignalHandler(int sigNum);

sig_atomic_t InitializeSignalStatus() {
  // Register the signal handler to gracefully shutdown the main loop for repeated requests.
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  return 0;
}

volatile sig_atomic_t g_signalStatus = InitializeSignalStatus();

}  // namespace

// According to the standard, 'SignalHandler' function should have C linkage:
// https://en.cppreference.com/w/cpp/utility/program/signal
// Thus it's not possible to use a lambda and pass some
// objects to it. This is why for this rare occasion we will rely on a static variable. This solution has been inspired
// by: https://wiki.sei.cmu.edu/confluence/display/cplusplus/MSC54-CPP.+A+signal+handler+must+be+a+plain+old+function
extern "C" void SignalHandler(int sigNum) {
  cct::log::warn("Signal {} received, will stop after current command", sigNum);

  g_signalStatus = sigNum;

  // Revert to standard signal handler (to allow for standard kill in case program does not react)
  std::signal(sigNum, SIG_DFL);
}

namespace cct {

bool IsStopRequested() { return g_signalStatus != 0; }

}  // namespace cct
