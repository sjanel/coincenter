#include "deposit-addresses.hpp"

#include <string_view>

#include "cct_const.hpp"
#include "file.hpp"
#include "read-json.hpp"

namespace cct {

schema::DepositAddresses ReadDepositAddresses(std::string_view dataDir) {
  return ReadJsonOrThrow<schema::DepositAddresses>(
      File{dataDir, File::Type::kSecret, kDepositAddressesFileName, File::IfError::kNoThrow});
}

}  // namespace cct