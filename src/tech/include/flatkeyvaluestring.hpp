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

/// Bi-directional iterator on {key,value} pairs of a FlatKeyValueString.
template <char KeyValuePairSep, char AssignmentChar>
class FlatKeyValueStringIterator {
  class FlatKeyValueIteratorValue {
   public:
    /// Get the key len of this iterator value
    auto keyLen() const { return _begValue - _begKey - 1U; }

    /// Get the value len of this iterator value
    auto valLen() const { return _endValue - _begValue; }

    /// Access to the key of this iterator value
    std::string_view key() const { return {_begKey, static_cast<std::string_view::size_type>(keyLen())}; }

    /// Access to the value of this iterator value
    std::string_view val() const { return {_begValue, _endValue}; }

    /// Get the total size of the key value pair of this iterator value
    auto size() const { return _endValue - _begKey; }

    /// a synonym of size()
    auto length() const { return size(); }

   private:
    template <char, char>
    friend class FlatKeyValueStringIterator;

    template <char, char>
    friend class FlatKeyValueString;

    /// begin()
    explicit FlatKeyValueIteratorValue(std::string_view data)
        : _begKey(data.data()), _begValue(nullptr), _endValue(nullptr) {
      std::size_t assignCharPos = data.find(AssignmentChar);
      if (assignCharPos != std::string_view::npos) {
        _begValue = _begKey + assignCharPos + 1U;

        std::size_t nextKVCharSep = data.find(KeyValuePairSep, assignCharPos + 1);
        _endValue = nextKVCharSep == std::string_view::npos ? data.data() + data.size() : data.data() + nextKVCharSep;
      }
    }

    /// end()
    explicit FlatKeyValueIteratorValue(const char *endData)
        : _begKey(endData), _begValue(nullptr), _endValue(nullptr) {}

    void incr(const char *endData) {
      if (_endValue == endData) {
        // reached the end
        _begKey = _endValue;
      } else {
        _begKey = _endValue + 1;
        _begValue = _begKey;
        do {
          ++_begValue;
        } while (*_begValue != AssignmentChar);
        ++_begValue;

        _endValue = _begValue;

        while (*_endValue != '\0' && *_endValue != KeyValuePairSep) {
          ++_endValue;
        }
      }
    }

    void decr(std::string_view data) {
      if (_begKey == data.data() + data.size()) {
        _endValue = _begKey;
      } else {
        _endValue = _begKey - 1;
      }
      _begValue = _endValue;
      do {
        --_begValue;
      } while (*_begValue != AssignmentChar);

      _begKey = _begValue;
      ++_begValue;

      do {
        --_begKey;
      } while (*_begKey != KeyValuePairSep && _begKey != data.data());

      if (*_begKey == KeyValuePairSep) {
        ++_begKey;
      }
    }

    const char *_begKey;
    const char *_begValue;
    const char *_endValue;
  };

 public:
  // Needed types for iterators.
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = FlatKeyValueIteratorValue;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  // Prefix increment, should be called on a valid iterator, otherwise undefined behavior
  FlatKeyValueStringIterator &operator++() {
    _value.incr(_data.data() + _data.size());
    return *this;
  }

  // Postfix increment
  FlatKeyValueStringIterator operator++(int) {
    auto ret = *this;
    ++(*this);
    return ret;
  }

  // Prefix decrement, should be called on a valid iterator (in range (begin(), end()]), otherwise undefined behavior
  FlatKeyValueStringIterator &operator--() {
    _value.decr(_data);
    return *this;
  }

  // Postfix decrement
  FlatKeyValueStringIterator operator--(int) {
    auto ret = *this;
    --(*this);
    return ret;
  }

  reference operator*() const { return _value; }

  pointer operator->() const { return &this->operator*(); }

  bool operator==(const FlatKeyValueStringIterator &rhs) const noexcept { return _value._begKey == rhs._value._begKey; }
  bool operator!=(const FlatKeyValueStringIterator &rhs) const noexcept { return !(*this == rhs); }

 private:
  template <char, char>
  friend class FlatKeyValueString;

  /// Create a new FlatKeyValueStringIterator representing begin()
  explicit FlatKeyValueStringIterator(std::string_view data) : _data(data), _value(data) {}

  /// Create a new FlatKeyValueStringIterator representing end()
  /// bool as second parameter is only here to differentiate both constructors
  FlatKeyValueStringIterator(std::string_view data, [[maybe_unused]] bool isEndIt)
      : _data(data), _value(_data.data() + _data.size()) {}

  std::string_view _data;
  FlatKeyValueIteratorValue _value;
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

  struct KeyValuePair {
    using IntegralType = int64_t;

    std::string_view key;
    std::variant<std::string_view, IntegralType> val;
  };

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

  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  auto rbegin() const { return std::make_reverse_iterator(begin()); }
  auto rend() const { return std::make_reverse_iterator(end()); }

  auto crbegin() const { return rbegin(); }
  auto crend() const { return rend(); }

  /// Note the return by copy for front() and back()
  /// This is because operator* will return a reference to a field inside the iterator that is only temporary in this
  /// method. Returning a reference here would be dangling hence the copy (but it's cheap to copy here, only 3
  /// pointers).
  auto front() const { return *begin(); }
  auto back() const { return *(--end()); }

  /// Append a new value for a key. No check is done on a duplicate key.
  /// There are several ways to set values as arrays (and none is standard). Choose the method depending on your
  /// usage:
  ///   - "aKey[]=val1&aKey[]=val2" can be used with several appends (one per value) with the same key suffixed with
  ///   []
  ///     This method needs to be used for direct call as parameter string
  ///   - If this query string will be transformed into json, set a key only once, with each value suffixed by a ','
  ///     (even the last one)
  ///     Examples:
  ///       "val": value is a single string
  ///       "val,": value is an array of a single string
  ///       "val1,val2,": value is an array of two values val1 and val2
  void push_back(std::string_view key, std::string_view value);

  void push_back(std::string_view key, std::integral auto val) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(val)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), val);

    push_back(key, std::string_view(buf, ret.ptr));
  }

  void push_back(const KeyValuePair &kvPair);

  /// Appends content of other FlatKeyValueString into 'this'.
  /// No check is made on duplicated keys.
  void append(const FlatKeyValueString &rhs);

  /// Like push_back, but insert at beginning instead
  void push_front(std::string_view key, std::string_view value);

  void push_front(std::string_view key, std::integral auto i) {
    // + 1 for minus, +1 for additional partial ranges coverage
    char buf[std::numeric_limits<decltype(i)>::digits10 + 2];
    auto ret = std::to_chars(buf, std::end(buf), i);
    push_front(key, std::string_view(buf, ret.ptr));
  }

  void push_front(const KeyValuePair &kvPair);

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

  /// Erase key value pair at iterator.
  void erase(const_iterator it);

  /// Erases last key value pair.
  /// Undefined behavior if empty.
  void pop_back() { erase(--end()); }

  void underlyingBufferReserve(size_type capacity) { _data.reserve(capacity); }

  /// Finds the position of the given key, or string::npos if key is not present
  size_type find(std::string_view key) const noexcept { return Find(_data, key); }

  bool contains(std::string_view key) const noexcept { return find(key) != string::npos; }

  /// Get the value associated to given key, or an empty string if no value is found for this key.
  std::string_view get(std::string_view key) const { return Get(_data, key); }

  bool empty() const noexcept { return _data.empty(); }

  const char *c_str() const noexcept { return _data.c_str(); }

  void clear() noexcept { _data.clear(); }

  void swap(FlatKeyValueString &rhs) noexcept { _data.swap(rhs._data); }

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
    push_back(kv);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::push_back(std::string_view key, std::string_view value) {
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
inline void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::push_back(const KeyValuePair &kv) {
  switch (kv.val.index()) {
    case 0:
      push_back(kv.key, std::get<std::string_view>(kv.val));
      break;
    case 1:
      push_back(kv.key, std::get<typename KeyValuePair::IntegralType>(kv.val));
      break;
    default:
      unreachable();
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::append(const FlatKeyValueString &rhs) {
  if (!rhs._data.empty()) {
    if (!_data.empty()) {
      _data.push_back(KeyValuePairSep);
    }
    _data.append(rhs._data);
  }
}

template <char KeyValuePairSep, char AssignmentChar>
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::push_front(std::string_view key, std::string_view value) {
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
inline void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::push_front(const KeyValuePair &kv) {
  switch (kv.val.index()) {
    case 0:
      push_front(kv.key, std::get<std::string_view>(kv.val));
      break;
    case 1:
      push_front(kv.key, std::get<typename KeyValuePair::IntegralType>(kv.val));
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

  const std::size_t pos = find(key);
  if (pos == string::npos) {
    push_back(key, value);
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
void FlatKeyValueString<KeyValuePairSep, AssignmentChar>::erase(const_iterator it) {
  const char *beg = it._value._begKey;
  const char *end = it._value._endValue;

  if (end != _data.data() + _data.size()) {
    ++end;
  } else if (beg != _data.data()) {
    --beg;
  }

  _data.erase(_data.begin() + (beg - _data.data()), _data.begin() + (end - _data.data()));
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
  for (const auto &kv : *this) {
    const auto key = kv.key();
    const auto val = kv.val();

    auto valSize = val.size();
    if (valSize == 0 || val.back() != kArrayElemSepChar) {
      ret.emplace(key, val);
      continue;
    }

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
  }
  return ret;
}

template <char KeyValuePairSep, char AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>
FlatKeyValueString<KeyValuePairSep, AssignmentChar>::urlEncodeExceptDelimiters() const {
  string ret(3U * _data.size(), '\0');
  char *outCharIt = ret.data();
  for (char ch : _data) {
    if (isalnum(ch) || ch == '@' || ch == '.' || ch == '\\' || ch == '-' || ch == '_' || ch == ':' ||
        ch == KeyValuePairSep || ch == AssignmentChar) {
      *outCharIt++ = ch;
    } else {
#ifdef CCT_MSVC
      sprintf_s(outCharIt, 4, "%%%02X", static_cast<unsigned char>(ch));
#else
      std::sprintf(outCharIt, "%%%02X", static_cast<unsigned char>(ch));
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
  std::size_t operator()(const cct::FlatKeyValueString<KeyValuePairSep, AssignmentChar> &val) const {
    return std::hash<std::string_view>()(val.str());
  }
};
}  // namespace std
