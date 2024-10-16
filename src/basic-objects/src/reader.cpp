#include "reader.hpp"

#include <utility>

#include "cct_json-container.hpp"
#include "cct_string.hpp"
namespace cct {

json::container Reader::readAllJson() const {
  string dataS = readAll();
  if (dataS.empty()) {
    dataS.append("{}");
  }
  return json::container::parse(std::move(dataS));
}

}  // namespace cct