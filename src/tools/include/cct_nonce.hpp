#pragma once

#include <string>

namespace cct {

using Nonce = std::string;

/// Create a new string representation of a nonce.
Nonce Nonce_TimeSinceEpoch();
}  // namespace cct
