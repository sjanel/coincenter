#pragma once

#include <string>

namespace cct {

using Nonce = std::string;

/// Create a string representation of a nonce.
Nonce Nonce_TimeSinceEpoch();

/// Create a string representation of a literal nonce with date and time.
/// Example: '2021-06-01T14:44:13'
Nonce Nonce_LiteralDate();
}  // namespace cct
