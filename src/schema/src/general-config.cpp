#include "general-config.hpp"

#include "cct_const.hpp"
#include "file.hpp"
#include "read-json.hpp"

namespace cct {

namespace {
File GetGeneralConfigFile(std::string_view dataDir) {
  return {dataDir, File::Type::kStatic, "generalconfig.hpp", File::IfError::kNoThrow};
}
}  // namespace

schema::GeneralConfig ReadGeneralConfig(std::string_view dataDir) {
  return ReadJsonOrThrow<schema::GeneralConfig>(GetGeneralConfigFile(dataDir));
}

}  // namespace cct