#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <variant>

#include "cct_mathhelpers.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct {
namespace table {

static constexpr char kColumnSep = '|';
static constexpr char kLineSep = '-';

using size_type = uint32_t;

enum class AlignTo { kLeft, kRight };

/// Helper function to align to left or right
class Align {
 public:
  explicit Align(AlignTo alignTo) : fmt(alignTo == AlignTo::kLeft ? std::ios::left : std::ios::right) {}

 private:
  std::ios::fmtflags fmt;

  friend std::ostream &operator<<(std::ostream &os, const Align &arg) {
    os.setf(arg.fmt);
    return os;
  }
};

/// Cell in a SimpleTable.
/// Can currently hold only 3 types of values: a string, a string_view or an integral type.
/// TODO: add support of 'double' if needed
struct Cell {
 public:
  using IntegralType = int64_t;
  using value_type = std::variant<string, std::string_view, IntegralType>;

  explicit Cell(std::string_view v) : _data(v) {}

  explicit Cell(const char *v) : _data(std::string_view(v)) {}

  explicit Cell(const string &v) : _data(v) {}

  explicit Cell(string &&v) : _data(std::move(v)) {}

  explicit Cell(IntegralType v) : _data(v) {}

  size_type size() const noexcept {
    switch (_data.index()) {
      case 0:
        return std::get<string>(_data).size();
      case 1:
        return std::get<std::string_view>(_data).size();
      case 2:
        return static_cast<size_type>(ndigits(std::get<IntegralType>(_data)));
      default:
        return 0;
    }
  }

 private:
  friend class Row;

  void print(std::ostream &os, size_type maxCellWidth) const {
    os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);
    switch (_data.index()) {
      case 0:
        os << std::get<string>(_data);
        break;
      case 1:
        os << std::get<std::string_view>(_data);
        break;
      case 2: {
        char buf[ndigits(std::numeric_limits<IntegralType>::max()) + 2];  // +1 for '\0', +1 for '-'

        if (auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), std::get<IntegralType>(_data));
            ec == std::errc()) {
          os << std::string_view(std::begin(buf), ptr);
        }
        break;
      }
    }
    os << ' ' << kColumnSep;
  }

  value_type _data;
};

/// Row in a SimpleTable.
class Row {
 public:
  using value_type = Cell;
  using const_iterator = SmallVector<value_type, 4>::const_iterator;

  template <class... Args>
  explicit Row(Args &&...args) {
    // Usage of C++17 fold expressions to make it possible to set a Row directly from a variadic input arguments
    ([&](auto &&input) { _cells.emplace_back(std::forward<decltype(input)>(input)); }(std::forward<Args>(args)), ...);
  }

  const_iterator begin() const noexcept { return _cells.begin(); }
  const_iterator end() const noexcept { return _cells.end(); }

  void push_back(const Cell &c) { _cells.push_back(c); }
  void push_back(Cell &&c) { _cells.push_back(std::move(c)); }

  template <class... Args>
  value_type &emplace_back(Args &&...args) {
    return _cells.emplace_back(std::forward<Args &&>(args)...);
  }

  size_type size() const noexcept { return _cells.size(); }

  value_type &operator[](size_type cellPos) { return _cells[cellPos]; }
  const value_type &operator[](size_type cellPos) const { return _cells[cellPos]; }

 private:
  friend class SimpleTable;

  void print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const {
    os << kColumnSep;
    size_type columnPos = 0;
    for (const Cell &c : _cells) {
      c.print(os, maxWidthPerColumn[columnPos++]);
    }
    os << std::endl;
  }

  SmallVector<value_type, 4> _cells;
};

/// Simple, lightweight and fast table with dynamic number of columns.
/// No checks are made about the number of columns for each Row, it's up to client's responsibility to make sure they
/// match.
class SimpleTable {
 public:
  using value_type = Row;
  using const_iterator = vector<Row>::const_iterator;

  SimpleTable() noexcept = default;

  template <class... Args>
  explicit SimpleTable(Args &&...args) {
    _rows.emplace_back(std::forward<Args &&>(args)...);
  }

  const_iterator begin() const { return _rows.begin(); }
  const_iterator end() const { return _rows.end(); }

  void push_back(const Row &row) { _rows.push_back(row); }
  void push_back(Row &&row) { _rows.push_back(std::move(row)); }

  template <class... Args>
  value_type &emplace_back(Args &&...args) {
    return _rows.emplace_back(std::forward<Args &&>(args)...);
  }

  size_type size() const noexcept { return _rows.size(); }
  bool empty() const noexcept { return _rows.empty(); }

  void print(std::ostream &os = std::cout) const {
    if (_rows.empty()) {
      return;
    }
    // We assume that each row has same number of cells, no silly checks here
    const size_type nbColumns = _rows.front().size();
    SmallVector<uint16_t, 8> maxWidthPerColumn(nbColumns, 0);
    for (const Row &r : _rows) {
      for (size_type columnPos = 0; columnPos < nbColumns; ++columnPos) {
        maxWidthPerColumn[columnPos] =
            std::max(maxWidthPerColumn[columnPos], static_cast<uint16_t>(r[columnPos].size()));
      }
    }
    const size_type sumWidths = std::accumulate(maxWidthPerColumn.begin(), maxWidthPerColumn.end(), 0U);
    const size_type maxTableWidth = sumWidths + (nbColumns * 3) + 1;
    const string lineSep(maxTableWidth, kLineSep);

    os << lineSep << std::endl;

    bool printHeader = _rows.size() > 1U;
    for (const Row &r : _rows) {
      r.print(os, maxWidthPerColumn);
      if (printHeader) {
        os << lineSep << std::endl;
        printHeader = false;
      }
    }

    os << lineSep << std::endl;
  }

 private:
  vector<Row> _rows;
};
}  // namespace table
}  // namespace cct