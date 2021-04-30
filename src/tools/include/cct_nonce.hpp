#pragma once

#include <string>

namespace cct {

using Nonce = std::string;

/// Create a new Nonce which is the string representation of the number of milliseconds between now and Epoch time.
Nonce Nonce_TimeSinceEpoch();
}  // namespace cct
