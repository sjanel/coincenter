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

schema::DepositAddresses ReadDepositAddresses(std::string_view dataDir) {
  return ReadJsonOrThrow<schema::DepositAddresses>(GetDepositAddressesFile(dataDir));
}

}  // namespace cct