#pragma once

#include <compare>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#ifdef CCT_MSVC
#include <string>
#endif

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"

namespace cct {

/// Simple, lightweight and fast table with dynamic number of columns.
/// No checks are made about the number of columns for each Row, it's up to client's responsibility to make sure they
/// match.
/// The SimpleTable is made up of 'Row's, themselves made up of 'Cell's, themselves made of 'CellLine's.
/// SimpleTable, Row, Cell behave like standard C++ vector-like containers, with support of most common methods.
/// The first Row is constructed like any other, but it will print an additional line separator to appear like a header.
/// No line separator will be placed between two single line only cells.
/// However, multi line Rows ('Row' containing at least a 'Cell' with several 'CellLine's) will have line separators
/// before and after them.
/// Empty rows are legal and will force the print of a divider line.
/// See the unit test to have an overview of its usage and the look and feel of the print.
/// Example:
///
/// +---------------+----------+-----------------------+
/// | Amount        | Currency | This header is longer |
/// +---------------+----------+-----------------------+
/// | 1235          | EUR      | Nothing here          |
/// | 3456.78       | USD      | 42                    |
/// | -677234.67    | SUSHI    | -12                   |
/// | -677256340000 | KEBAB    | -34.09                |
/// |~~~~~~~~~~~~~~~|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~|
/// | 12            |          | Nothing here          |
/// | -4            |          |                       |
/// +---------------+----------+-----------------------+

namespace table {

/// Cell in a SimpleTable on a single line.
/// Can currently hold only 4 types of values: a string, a string_view, a int64_t and a bool.
/// Note: if you pass a const char * or a string_view, a string_view will be stored in the cell.
/// Make sure that the lifetime of the data it points to extends the lifetime of this cell.
class CellLine {
 public:
#ifdef CCT_MSVC
  // folly::string does not support operator<< correctly with alignments with MSVC. Hence we use std::string
  // in SimpleTable to guarantee correct alignment of formatted table. Referenced in this issue:
  // https://github.com/facebook/folly/issues/1681
  using string_type = std::string;
#else
  using string_type = string;
#endif
  using value_type = std::variant<std::string_view, string_type, int64_t, uint64_t, bool>;
  using size_type = uint32_t;

  explicit CellLine(std::string_view sv) : _data(sv) {}

  explicit CellLine(const char *cstr) : _data(std::string_view(cstr)) {}

#ifdef CCT_MSVC
  explicit CellLine(const string &v) : _data(string_type(v.data(), v.size())) {}
#else
  explicit CellLine(const string_type &str) : _data(str) {}

  explicit CellLine(string_type &&str) : _data(std::move(str)) {}
#endif

  explicit CellLine(std::integral auto val) : _data(val) {}

  // Number of chars of this single line cell value.
  size_type width() const;

  void swap(CellLine &rhs) noexcept { _data.swap(rhs._data); }

  using trivially_relocatable = is_trivially_relocatable<string_type>::type;

  std::strong_ordering operator<=>(const CellLine &) const noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const CellLine &singleLineCell);

 private:
  value_type _data;
};

class Row;
class Cell {
 public:
  using value_type = CellLine;
  using size_type = uint32_t;

 private:
  using CellLineVector = SmallVector<value_type, 1>;

 public:
  using iterator = CellLineVector::iterator;
  using const_iterator = CellLineVector::const_iterator;

  /// Implicit constructor of a Cell from a CellLine.
  Cell(CellLine singleLineCell) { _singleLineCells.push_back(std::move(singleLineCell)); }

  /// Creates a new Row with given list of cells.
  template <class... Args>
  explicit Cell(Args &&...singleLineCells) {
    ([&](auto &&input) { _singleLineCells.emplace_back(std::forward<decltype(input)>(input)); }(
         std::forward<Args>(singleLineCells)),
     ...);
  }

  iterator begin() noexcept { return _singleLineCells.begin(); }
  const_iterator begin() const noexcept { return _singleLineCells.begin(); }

  iterator end() noexcept { return _singleLineCells.end(); }
  const_iterator end() const noexcept { return _singleLineCells.end(); }

  value_type &front() { return _singleLineCells.front(); }
  const value_type &front() const { return _singleLineCells.front(); }

  value_type &back() { return _singleLineCells.back(); }
  const value_type &back() const { return _singleLineCells.back(); }

  void push_back(const value_type &cell) { _singleLineCells.push_back(cell); }
  void push_back(value_type &&cell) { _singleLineCells.push_back(std::move(cell)); }

  template <class... Args>
  value_type &emplace_back(Args &&...args) {
    return _singleLineCells.emplace_back(std::forward<Args &&>(args)...);
  }

  size_type size() const noexcept { return _singleLineCells.size(); }

  size_type width() const;

  value_type &operator[](size_type cellPos) { return _singleLineCells[cellPos]; }
  const value_type &operator[](size_type cellPos) const { return _singleLineCells[cellPos]; }

  void reserve(size_type sz) { _singleLineCells.reserve(sz); }

  void swap(Cell &rhs) noexcept { _singleLineCells.swap(rhs._singleLineCells); }

  using trivially_relocatable = is_trivially_relocatable<CellLineVector>::type;

  std::strong_ordering operator<=>(const Cell &) const noexcept = default;

 private:
  CellLineVector _singleLineCells;
};

/// Row in a SimpleTable.
class Row {
 public:
  using value_type = Cell;
  using size_type = uint32_t;

 private:
  using CellVector = vector<value_type>;

 public:
  using iterator = CellVector::iterator;
  using const_iterator = CellVector::const_iterator;

  /// Creates a new Row with given list of cells.
  template <class... Args>
  explicit Row(Args &&...cells) {
    ([&](auto &&input) { _cells.emplace_back(std::forward<decltype(input)>(input)); }(std::forward<Args>(cells)), ...);
  }

  iterator begin() noexcept { return _cells.begin(); }
  const_iterator begin() const noexcept { return _cells.begin(); }

  iterator end() noexcept { return _cells.end(); }
  const_iterator end() const noexcept { return _cells.end(); }

  value_type &front() { return _cells.front(); }
  const value_type &front() const { return _cells.front(); }

  value_type &back() { return _cells.back(); }
  const value_type &back() const { return _cells.back(); }

  void push_back(const value_type &cell) { _cells.push_back(cell); }
  void push_back(value_type &&cell) { _cells.push_back(std::move(cell)); }

  template <class... Args>
  value_type &emplace_back(Args &&...args) {
    return _cells.emplace_back(std::forward<Args &&>(args)...);
  }

  size_type size() const noexcept { return _cells.size(); }

  bool empty() const noexcept { return _cells.empty(); }

  void reserve(size_type sz) { _cells.reserve(sz); }

  value_type &operator[](size_type cellPos) { return _cells[cellPos]; }
  const value_type &operator[](size_type cellPos) const { return _cells[cellPos]; }

  void swap(Row &rhs) noexcept { _cells.swap(rhs._cells); }

  using trivially_relocatable = is_trivially_relocatable<CellVector>::type;

  std::strong_ordering operator<=>(const Row &) const noexcept = default;

 private:
  CellVector _cells;
};

}  // namespace table

using SimpleTable = vector<table::Row>;

std::ostream &operator<<(std::ostream &os, const SimpleTable &table);
}  // namespace cct