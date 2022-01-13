#pragma once

#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

#include "cct_hash.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "unreachable.hpp"

namespace cct {

struct KeyValuePair {
  using IntegralType = int64_t;
  using value_type = std::variant<string, std::string_view, IntegralType>;

  KeyValuePair(std::string_view k, std::string_view v) : key(k), val(v) {}

  KeyValuePair(std::string_view k, const char *v) : key(k), val(std::string_view(v)) {}

  KeyValuePair(std::string_view k, string &&v) : key(k), val(std::move(v)) {}

  KeyValuePair(std::string_view k, IntegralType v) : key(k), val(v) {}

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  std::string_view key;
  value_type val;
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
      _kv[0] = std::string_view(_data.end(), _data.end());
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

  bool operator==(const FlatKeyValueStringIterator &o) const { return _kv[0].data() == o._kv[0].data(); }
  bool operator!=(const FlatKeyValueStringIterator &o) const { return !(*this == o); }

 private:
  template <char, char>
  friend class FlatKeyValueString;

  /// Create a new FlatKeyValueStringIterator pointing at beginning of data
  explicit FlatKeyValueStringIterator(std::string_view data) : _data(data) {
    std::size_t assignCharPos = _data.find(AssignmentChar);
    if (assignCharPos == std::string_view::npos) {
      // end
      _kv[0] = std::string_view(_data.end(), _data.end());
    } else {
      _kv[0] = std::string_view(_data.begin(), _data.begin() + assignCharPos);
      std::size_t nextKVCharSep = _data.find(KeyValuePairSep, assignCharPos + 1);
      _kv[1] = std::string_view(_data.begin() + assignCharPos + 1,
                                nextKVCharSep == std::string_view::npos ? _data.end() : _data.begin() + nextKVCharSep);
    }
  }

  /// Create a new FlatKeyValueStringIterator pointing at end of data
  FlatKeyValueStringIterator(std::string_view data, bool)
      : _data(data), _kv({std::string_view(_data.end(), _data.end()), std::string_view()}) {}

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

  static constexpr char kArrayElemSepChar = ',';

  FlatKeyValueString() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  FlatKeyValueString(std::initializer_list<KeyValuePair> init)
      : FlatKeyValueString(std::span<const KeyValuePair>(init.begin(), init.end())) {}

  explicit FlatKeyValueString(std::span<const KeyValuePair> init);

  explicit FlatKeyValueString(string &&o) noexcept(std::is_nothrow_move_constructible_v<string>)
      : _data(std::move(o)) {}

  const_iterator begin() const { return const_iterator(_data); }
  const_iterator end() const { return const_iterator(_data, true); }

  /// Append a new value for a key.
  /// Key should not already be present.
  /// If you wish to convert value to array (for json conversion), you can end each value with a ','.
  /// Examples:
  ///   "val": value is a single string
  ///   "val,": value is an array of a single string
  ///   "val1,val2,": value is an array of two values val1 and val2
  void append(std::string_view key, std::string_view value);

  void append(std::string_view key, std::integral auto i) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(i)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), i);
    append(key, std::string_view(buf, ret.ptr));
  }

  /// Appends content of other FlatKeyValueString into 'this'.
  /// No check is made on duplicated keys, it is client's responsibility to make sure keys are not duplicated.
  void append(const FlatKeyValueString &o);

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

  /// Finds the position of the given key
  std::size_t find(std::string_view key) const noexcept;

  bool contains(std::string_view key) const noexcept { return find(key) != string::npos; }

  /// Get the value associated to given key
  std::string_view get(std::string_view key) const;

  bool empty() const noexcept { return _data.empty(); }

  const char *c_str() const noexcept { return _data.c_str(); }

  void clear() noexcept { _data.clear(); }

  std::string_view str() const noexcept { return _data; }

  /// Converts to a json document.
  /// Values ending with a ',' will be considered as arrays.
  /// In this case, sub array values are comma separated values.
  /// Limitation: all json values will be decoded as strings.
  json toJson() const;

  auto operator<=>(const FlatKeyValueString &) const = default;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  string _data;
};

template <char KeyValuePairSep, char AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>::FlatKeyValueString(std::span<const KeyValuePair> init) {
  for (const KeyValuePair &kv : init) {
    switch (kv.val.index()) {
      case 0:
        append(kv.key, std::get<string>(kv.val));
        break;
      case 1:
        append(kv.key, std::get<std::string_view>(kv.val));
        break;
      case 2:
        append(kv.key, std::get<KeyValuePair::IntegralType>(kv.val));
        break;
      default:
        unreachable();
    }
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::append(std::string_view key, std::string_view value) {
  assert(!key.empty());
  assert(!value.empty());
  assert(key.find(KeyValuePairSep) == std::string_view::npos);
  assert(key.find(AssignmentChar) == std::string_view::npos);
  assert(value.find(KeyValuePairSep) == std::string_view::npos);
  assert(value.find(AssignmentChar) == std::string_view::npos);
  assert(!contains(key));
  if (!_data.empty()) {
    _data.push_back(KeyValuePairSep);
  }
  _data.append(key);
  _data.push_back(AssignmentChar);
  _data.append(value);
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
std::size_t FlatKeyValueString<KeyValuePairSep, AssignmentChar>::find(std::string_view key) const noexcept {
  const std::size_t ks = key.size();
  const std::size_t ps = _data.size();
  std::size_t pos = _data.find(key);
  while (pos != string::npos && pos + ks < ps && _data[pos + ks] != AssignmentChar) {
    pos = _data.find(key, pos + ks + 1);
  }
  if (pos != string::npos && (pos + ks == ps || _data[pos + ks] == KeyValuePairSep)) {
    // we found a value, not a key
    pos = string::npos;
  }
  return pos;
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::set(std::string_view key, std::string_view value) {
  assert(!key.empty());
  assert(!value.empty());
  assert(key.find(KeyValuePairSep) == std::string_view::npos);
  assert(key.find(AssignmentChar) == std::string_view::npos);
  assert(value.find(KeyValuePairSep) == std::string_view::npos);
  assert(value.find(AssignmentChar) == std::string_view::npos);
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
    _data.erase(_data.begin() + first, _data.begin() + last + (first == 0));
  }
}

template <char KeyValuePairSep, char AssignmentChar>
std::string_view FlatKeyValueString<KeyValuePairSep, AssignmentChar>::get(std::string_view key) const {
  std::size_t pos = find(key);
  string::const_iterator first;
  string::const_iterator last;
  if (pos == string::npos) {
    first = _data.end();
    last = _data.end();
  } else {
    first = _data.begin() + pos + key.size() + 1;
    std::size_t endPos = _data.find(KeyValuePairSep, pos + key.size() + 1);
    if (endPos == string::npos) {
      last = _data.end();
    } else {
      last = _data.begin() + endPos;
    }
  }
  return std::string_view(first, last);
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

}  // namespace cct

namespace std {
template <char KeyValuePairSep, char AssignmentChar>
struct hash<cct::FlatKeyValueString<KeyValuePairSep, AssignmentChar>> {
  std::size_t operator()(const cct::FlatKeyValueString<KeyValuePairSep, AssignmentChar> &v) const {
    return std::hash<std::string_view>()(v.str());
  }
};
}  // namespace std
