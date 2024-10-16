#include "writer.hpp"

#include <string_view>

#include "cct_json-container.hpp"
namespace cct {

int Writer::writeJson(const json::container &data, Mode mode) const {
  if (data.empty()) {
    return this->write(std::string_view("{}"), mode);
  }
  const int indent = mode == Writer::Mode::FromStart ? 2 : -1;
  const auto str = data.dump(indent);
  return this->write(static_cast<std::string_view>(str), mode);
}

}  // namespace cct