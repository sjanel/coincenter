#include "curloptions.hpp"

#include <cassert>

#include "cct_config.hpp"
#include "cct_exception.hpp"

namespace cct {

CurlPostData::CurlPostData(std::initializer_list<KeyValuePair> init) {
  for (const auto& [k, v] : init) {
    append(k, v);
  }
}

CurlPostData& CurlPostData::operator=(std::initializer_list<KeyValuePair> init) {
  clear();
  for (const auto& [k, v] : init) {
    append(k, v);
  }
  return *this;
}

void CurlPostData::append(std::string_view key, std::string_view value) {
  assert(!key.empty() && !value.empty() && key.find('&') == std::string_view::npos &&
         value.find('&') == std::string_view::npos && key.find('=') == std::string_view::npos &&
         value.find('=') == std::string_view::npos);
  if (!_postdata.empty()) {
    if (CCT_UNLIKELY(contains(key))) {
      throw exception("Cannot append twice the same key");
    }
    _postdata.push_back('&');
  }
  _postdata.append(key);
  _postdata.push_back('=');
  _postdata.append(value);
}

std::size_t CurlPostData::find(std::string_view key) const {
  const std::size_t ks = key.size();
  const std::size_t ps = _postdata.size();
  std::size_t pos = _postdata.find(key);
  while (pos != std::string::npos && pos + ks < ps && _postdata[pos + ks] != '=') {
    pos = _postdata.find(key, pos + ks + 1);
  }
  if (pos + ks == ps || _postdata[pos + ks] == '&') {
    // we found a value, not a key
    pos = std::string::npos;
  }
  return pos;
}

void CurlPostData::set(std::string_view key, std::string_view value) {
  assert(!key.empty() && !value.empty() && key.find('&') == std::string_view::npos &&
         value.find('&') == std::string_view::npos && key.find('=') == std::string_view::npos &&
         value.find('=') == std::string_view::npos);
  std::size_t pos = find(key);
  if (pos == std::string::npos) {
    append(key, value);
  } else {
    pos += key.size() + 1;
    std::size_t endPos = pos + 1;
    const std::size_t ps = _postdata.size();
    while (endPos < ps && _postdata[endPos] != '&') {
      ++endPos;
    }
    _postdata.replace(pos, endPos - pos, value);
  }
}

void CurlPostData::erase(std::string_view key) {
  std::size_t first = find(key);
  if (first != std::string::npos) {
    if (first != 0) {
      --first;
    }
    std::size_t last = first + key.size() + 2;
    const std::size_t ps = _postdata.size();
    while (last < ps && _postdata[last] != '&') {
      ++last;
    }
    _postdata.erase(_postdata.begin() + first, _postdata.begin() + last);
  }
}

std::string_view CurlPostData::get(std::string_view key) const {
  std::size_t pos = find(key);
  std::string::const_iterator first;
  std::string::const_iterator last;
  if (pos == std::string::npos) {
    first = _postdata.end();
    last = _postdata.end();
  } else {
    first = _postdata.begin() + pos + key.size() + 1;
    std::size_t endPos = _postdata.find_first_of('&', pos + key.size() + 1);
    if (endPos == std::string::npos) {
      last = _postdata.end();
    } else {
      last = _postdata.begin() + endPos;
    }
  }
  return std::string_view(first, last);
}

}  // namespace cct