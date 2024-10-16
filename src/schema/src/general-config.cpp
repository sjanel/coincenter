#include "general-config.hpp"

#include <string_view>

#include "file.hpp"
#include "read-json.hpp"

namespace cct {

schema::GeneralConfig ReadGeneralConfig(std::string_view dataDir) {
  return ReadJsonOrCreateFile<schema::GeneralConfig>(
      File{dataDir, File::Type::kStatic, "generalconfig.json", File::IfError::kNoThrow});
}

}  // namespace cct