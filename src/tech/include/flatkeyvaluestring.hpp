#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "cct_cctype.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "unreachable.hpp"

namespace cct {

struct KeyValuePair {
  using IntegralType = int64_t;

  std::string_view key;
  std::variant<std::string_view, IntegralType> val;
};

template <char KeyValuePairSep, char AssignmentChar>
class FlatKeyValueStringIterator {
 public:
  // Needed types for iterators.
  // It could be easily transformed into a Bi directional iterator but I did not find yet the need to iterate
  // backwards.
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::array<std::string_view, 2>;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  // Prefix increment, should be called on a valid iterator, otherwise undefined behavior
  FlatKeyValueStringIterator &operator++() {
    // Use .data() + size() method instead of end() iterators as they may not be implemented as pointers
    if (_kv[1].data() + _kv[1].size() == _data.data() + _data.size()) {
      // end
      _kv[0] = std::string_view();
    } else {
      // There is a next key value pair
      const char *start = _kv[1].data() + _kv[1].size() + 1;
      const char *end = start + 1;
      while (*end != AssignmentChar) {
        ++end;
      }
      _kv[0] = std::string_view(start, end - start);
      start = end + 1;
      while (*end != '\0' && *end != KeyValuePairSep) {
        ++end;
      }
      _kv[1] = std::string_view(start, end - start);
    }
    return *this;
  }

  const value_type &operator*() const { return _kv; }
  const value_type *operator->() const { return &this->operator*(); }

  bool operator==(const FlatKeyValueStringIterator &rhs) const noexcept { return _kv[0].data() == rhs._kv[0].data(); }
  bool operator!=(const FlatKeyValueStringIterator &rhs) const noexcept { return !(*this == rhs); }

 private:
  template <char, char>
  friend class FlatKeyValueString;

  /// Create a new FlatKeyValueStringIterator pointing at beginning of data
  explicit FlatKeyValueStringIterator(std::string_view data) : _data(data) {
    std::size_t assignCharPos = _data.find(AssignmentChar);
    if (assignCharPos != std::string_view::npos) {  // equal is end
      _kv[0] = std::string_view(_data.begin(), _data.begin() + assignCharPos);
      std::size_t nextKVCharSep = _data.find(KeyValuePairSep, assignCharPos + 1);
      _kv[1] = std::string_view(_data.begin() + assignCharPos + 1,
                                nextKVCharSep == std::string_view::npos ? _data.end() : _data.begin() + nextKVCharSep);
    }
  }

  /// Create a new FlatKeyValueStringIterator representing end()
  /// bool as second parameter is only here to differentiate both constructors
  FlatKeyValueStringIterator(std::string_view data, [[maybe_unused]] bool dummy) : _data(data) {}

  std::string_view _data;
  value_type _kv;
};

/// String Key / Value pairs flattened in a single string.
/// It can be used to store URL parameters for instance, or as an optimized key for a map / hashmap based on a list of
/// key value pairs.
/// A value can be simulated as an array of elements separated by AssignmentChar, useful for json conversion
template <char KeyValuePairSep, char AssignmentChar>
class FlatKeyValueString {
 public:
  using iterator = FlatKeyValueStringIterator<KeyValuePairSep, AssignmentChar>;
  using const_iterator = iterator;
  using size_type = string::size_type;

  static constexpr char kArrayElemSepChar = ',';

  /// Finds the position of the given key in given data, or string::npos if key is not present
  static size_type Find(std::string_view data, std::string_view key);

  /// Get the value associated to given key, or an empty string if no value is found for this key.
  static std::string_view Get(std::string_view data, std::string_view key);

  FlatKeyValueString() noexcept = default;

  FlatKeyValueString(std::initializer_list<KeyValuePair> init)
      : FlatKeyValueString(std::span<const KeyValuePair>(init.begin(), init.end())) {}

  explicit FlatKeyValueString(std::span<const KeyValuePair> init);

  const_iterator begin() const { return const_iterator(_data); }
  const_iterator end() const { return const_iterator(_data, true); }

  /// Append a new value for a key. No check is done on a duplicate key.
  /// There are several ways to set values as arrays (and none is standard). Choose the method depending on your usage:
  ///   - "aKey[]=val1&aKey[]=val2" can be used with several appends (one per value) with the same key suffixed with []
  ///     This method needs to be used for direct call as parameter string
  ///   - If this query string will be transformed into json, set a key only once, with each value suffixed by a ','
  ///     (even the last one)
  ///     Examples:
  ///       "val": value is a single string
  ///       "val,": value is an array of a single string
  ///       "val1,val2,": value is an array of two values val1 and val2
  void append(std::string_view key, std::string_view value);

  void append(std::string_view key, std::integral auto i) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(i)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), i);
    append(key, std::string_view(buf, ret.ptr));
  }

  void append(const KeyValuePair &kvPair);

  /// Appends content of other FlatKeyValueString into 'this'.
  /// No check is made on duplicated keys, it is client's responsibility to make sure keys are not duplicated.
  void append(const FlatKeyValueString &o);

  /// Like append, but insert at beginning instead
  void prepend(std::string_view key, std::string_view value);

  void prepend(std::string_view key, std::integral auto i) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(i)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), i);
    prepend(key, std::string_view(buf, ret.ptr));
  }

  void prepend(const KeyValuePair &kvPair);

  /// Updates the value for given key, or append if not existing.
  void set(std::string_view key, std::string_view value);

  void set(std::string_view key, std::integral auto i) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(i)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), i);
    set(key, std::string_view(buf, ret.ptr));
  }

  /// Erases given key if present.
  void erase(std::string_view key);

  void reserve(size_type capacity) { _data.reserve(capacity); }

  /// Finds the position of the given key, or string::npos if key is not present
  size_type find(std::string_view key) const noexcept { return Find(_data, key); }

  bool contains(std::string_view key) const noexcept { return find(key) != string::npos; }

  /// Get the value associated to given key, or an empty string if no value is found for this key.
  std::string_view get(std::string_view key) const { return Get(_data, key); }

  bool empty() const noexcept { return _data.empty(); }

  const char *c_str() const noexcept { return _data.c_str(); }

  void clear() noexcept { _data.clear(); }

  /// Get a string_view on the full data hold by this FlatKeyValueString.
  /// The returned string_view is guaranteed to be null-terminated.
  std::string_view str() const noexcept { return _data; }

  /// Converts to a json document.
  /// Values ending with a ',' will be considered as arrays.
  /// In this case, sub array values are comma separated values.
  /// Limitation: all json values will be decoded as strings.
  json toJson() const;

  /// Returns a new FlatKeyValueString URL encoded except delimiters.
  FlatKeyValueString urlEncodeExceptDelimiters() const;

  auto operator<=>(const FlatKeyValueString &) const = default;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  explicit FlatKeyValueString(string &&data) noexcept(std::is_nothrow_move_constructible_v<string>)
      : _data(std::move(data)) {}

  string _data;
};

template <char KeyValuePairSep, char AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>::FlatKeyValueString(std::span<const KeyValuePair> init) {
  for (const KeyValuePair &kv : init) {
    append(kv);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::append(std::string_view key, std::string_view value) {
  assert(!key.empty());
  assert(!value.empty());
  assert(key.find(KeyValuePairSep) == std::string_view::npos);
  assert(key.find(AssignmentChar) == std::string_view::npos);
  assert(value.find(KeyValuePairSep) == std::string_view::npos);
  std::size_t pos = _data.size();
  _data.append((pos == 0 ? 0U : 1U) + key.size() + 1U + value.size(), AssignmentChar);
  if (pos != 0) {
    _data[pos] = KeyValuePairSep;
    ++pos;
  }
  auto it = std::ranges::copy(key, _data.begin() + pos).out;
  std::ranges::copy(value, it + 1);
}

template <char KeyValuePairSep, char AssignmentChar>
inline void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::append(const KeyValuePair &kv) {
  switch (kv.val.index()) {
    case 0:
      append(kv.key, std::get<std::string_view>(kv.val));
      break;
    case 1:
      append(kv.key, std::get<KeyValuePair::IntegralType>(kv.val));
      break;
    default:
      unreachable();
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::append(const FlatKeyValueString &o) {
  if (!o._data.empty()) {
    if (!_data.empty()) {
      _data.push_back(KeyValuePairSep);
    }
    _data.append(o._data);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::prepend(std::string_view key, std::string_view value) {
  assert(!key.empty());
  assert(!value.empty());
  assert(key.find(KeyValuePairSep) == std::string_view::npos);
  assert(key.find(AssignmentChar) == std::string_view::npos);
  assert(value.find(KeyValuePairSep) == std::string_view::npos);
  if (_data.empty()) {
    _data.append(key);
    _data.push_back(AssignmentChar);
    _data.append(value);
  } else {
    _data.insert(0U, key.size() + value.size() + 2U, KeyValuePairSep);
    std::ranges::copy(key, _data.begin());
    _data[key.size()] = AssignmentChar;
    std::ranges::copy(value, _data.begin() + key.size() + 1U);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
inline void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::prepend(const KeyValuePair &kv) {
  switch (kv.val.index()) {
    case 0:
      prepend(kv.key, std::get<std::string_view>(kv.val));
      break;
    case 1:
      prepend(kv.key, std::get<KeyValuePair::IntegralType>(kv.val));
      break;
    default:
      unreachable();
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::set(std::string_view key, std::string_view value) {
  assert(!key.empty());
  assert(!value.empty());
  assert(key.find(KeyValuePairSep) == std::string_view::npos);
  assert(key.find(AssignmentChar) == std::string_view::npos);
  assert(value.find(KeyValuePairSep) == std::string_view::npos);
  std::size_t pos = find(key);
  if (pos == string::npos) {
    append(key, value);
  } else {
    string::const_iterator first = _data.begin() + pos + key.size() + 1;
    string::const_iterator last = first + 1;
    string::const_iterator end = _data.end();
    while (last != end && *last != KeyValuePairSep) {
      ++last;
    }
    _data.replace(first, last, value);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::erase(std::string_view key) {
  std::size_t first = find(key);
  if (first != string::npos) {
    if (first != 0) {
      --first;  // Remove separator char as well
    }
    std::size_t last = first + key.size() + 2;
    const std::size_t ps = _data.size();
    while (last < ps && _data[last] != KeyValuePairSep) {
      ++last;
    }
    _data.erase(_data.begin() + first, _data.begin() + last + static_cast<std::size_t>(first == 0));
  }
}

template <char KeyValuePairSep, char AssignmentChar>
std::size_t FlatKeyValueString<KeyValuePairSep, AssignmentChar>::Find(std::string_view data, std::string_view key) {
  const std::size_t ks = key.size();
  const std::size_t ds = data.size();
  // Ideally, we would like to search for key + AssignmentChar, but we don't want to make a new string
  std::size_t pos = data.find(key);
  while (pos != string::npos && pos + ks < ds && data[pos + ks] != AssignmentChar) {
    pos = data.find(key, pos + ks + 1);
  }
  if (pos != string::npos && (pos + ks == ds || data[pos + ks] == KeyValuePairSep)) {
    // we found a value, not a key
    pos = string::npos;
  }
  return pos;
}

template <char KeyValuePairSep, char AssignmentChar>
std::string_view FlatKeyValueString<KeyValuePairSep, AssignmentChar>::Get(std::string_view data, std::string_view key) {
  std::size_t pos = Find(data, key);
  std::string_view::const_iterator first;
  std::string_view::const_iterator last;
  if (pos == string::npos) {
    first = data.end();
    last = data.end();
  } else {
    first = data.begin() + pos + key.size() + 1;
    std::size_t endPos = data.find(KeyValuePairSep, pos + key.size() + 1);
    if (endPos == string::npos) {
      last = data.end();
    } else {
      last = data.begin() + endPos;
    }
  }
  return {first, last};
}

template <char KeyValuePairSep, char AssignmentChar>
json FlatKeyValueString<KeyValuePairSep, AssignmentChar>::toJson() const {
  json ret;
  for (const auto &[key, val] : *this) {
    auto valSize = val.size();
    bool valIsArray = valSize != 0 && val.back() == kArrayElemSepChar;
    if (valIsArray) {
      vector<string> arrayValues;
      if (valSize != 1U) {  // Check empty array case
        for (std::size_t arrayValBeg = 0;;) {
          std::size_t arrayValSepPos = val.find(kArrayElemSepChar, arrayValBeg);
          arrayValues.emplace_back(std::string_view(val.begin() + arrayValBeg, val.begin() + arrayValSepPos));
          if (arrayValSepPos + 1U == valSize) {
            break;
          }
          arrayValBeg = arrayValSepPos + 1;
        }
      }

      ret.emplace(key, std::move(arrayValues));
    } else {
      ret.emplace(key, val);
    }
  }
  return ret;
}

template <char KeyValuePairSep, char AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>::urlEncodeExceptDelimiters() const {
  string ret(3U * _data.size(), '\0');
  char *outCharIt = ret.data();
  for (char c : _data) {
    if (isalnum(c) || c == '@' || c == '.' || c == '\\' || c == '-' || c == '_' || c == ':' || c == KeyValuePairSep ||
        c == AssignmentChar) {
      *outCharIt++ = c;
    } else {
#ifdef CCT_MSVC
      sprintf_s(outCharIt, 4, "%%%02X", static_cast<unsigned char>(c));
#else
      std::sprintf(outCharIt, "%%%02X", static_cast<unsigned char>(c));
#endif
      outCharIt += 3;
    }
  }
  ret.resize(outCharIt - ret.data());
  return FlatKeyValueString<KeyValuePairSep, AssignmentChar>(std::move(ret));
}

}  // namespace cct

namespace std {
template <char KeyValuePairSep, char AssignmentChar>
struct hash<cct::FlatKeyValueString<KeyValuePairSep, AssignmentChar>> {
  std::size_t operator()(const cct::FlatKeyValueString<KeyValuePairSep, AssignmentChar> &v) const {
    return std::hash<std::string_view>()(v.str());
  }
};
}  // namespace std
