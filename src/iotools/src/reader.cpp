#include "reader.hpp"

namespace cct {

json Reader::readAllJson() const {
  string dataS = readAll();
  if (dataS.empty()) {
    dataS = "{}";
  }
  return json::parse(std::move(dataS));
}

}  // namespace cct