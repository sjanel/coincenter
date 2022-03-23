#include "exchangename.hpp"

#include "cct_exception.hpp"
#include "toupperlower.hpp"

namespace cct {
PrivateExchangeName::PrivateExchangeName(std::string_view globalExchangeName) : _nameWithKey(globalExchangeName) {
  for (std::size_t p = 0, s = globalExchangeName.size(); p < s && _nameWithKey[p] != '_'; ++p) {
    _nameWithKey[p] = tolower(_nameWithKey[p]);
  }
}

PrivateExchangeName::PrivateExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(ToLower(exchangeName)) {
  if (_nameWithKey.find('_') != string::npos) {
    string err("Invalid exchange name ");
    err.append(_nameWithKey);
    throw exception(std::move(err));
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct