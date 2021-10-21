#pragma once

#include "cct_string.hpp"

namespace cct {

using Nonce = string;

/// Create a string representation of a nonce.
Nonce Nonce_TimeSinceEpoch();

/// Create a string representation of a literal nonce with date and time.
/// Example: '2021-06-01T14:44:13'
Nonce Nonce_LiteralDate();
}  // namespace cct
