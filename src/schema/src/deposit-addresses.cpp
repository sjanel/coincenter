#include "deposit-addresses.hpp"

#include "cct_const.hpp"
#include "file.hpp"
#include "read-json.hpp"

namespace cct {

namespace {
File GetDepositAddressesFile(std::string_view dataDir) {
  return {dataDir, File::Type::kSecret, kDepositAddressesFileName, File::IfError::kNoThrow};
}
}  // namespace

DepositAddresses ReadDepositAddresses(std::string_view dataDir) {
  return ReadJsonOrThrow<DepositAddresses>(GetDepositAddressesFile(dataDir));
}

}  // namespace cct