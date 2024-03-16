#include "simpletable.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <numeric>
#include <ostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "mathhelpers.hpp"

namespace cct {

/// A divider row is represented as an empty row.
/// It will be treated specially in the print
const SimpleTable::Row SimpleTable::Row::kDivider;

namespace {
constexpr char kColumnSep = '|';

constexpr std::string_view kBoolValueTrue = "yes";
constexpr std::string_view kBoolValueFalse = "no";
constexpr char kEmptyValueChar = ' ';

enum class AlignTo : int8_t { kLeft, kRight };

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

template <class>
constexpr bool always_false_v = false;
}  // namespace

SimpleTable::size_type SimpleTable::CellLine::width() const {
  return std::visit(
      [](auto &&val) -> size_type {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, string_type> || std::is_same_v<T, std::string_view>) {
          return val.length();
        } else if constexpr (std::is_same_v<T, bool>) {
          return val ? kBoolValueTrue.length() : kBoolValueFalse.length();
        } else if constexpr (std::is_integral_v<T>) {
          return nchars(val);
        } else {
          // Note: can be replaced with 'static_assert(false);' in C++23
          static_assert(always_false_v<T>, "non-exhaustive visitor!");
        }
      },
      _data);
}

std::ostream &operator<<(std::ostream &os, const SimpleTable::CellLine &singleLineCell) {
  std::visit(
      [&os](auto &&val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, bool>) {
          os << (val ? kBoolValueTrue : kBoolValueFalse);
        } else if constexpr (std::is_same_v<T, SimpleTable::CellLine::string_type> ||
                             std::is_same_v<T, std::string_view> || std::is_integral_v<T>) {
          os << val;
        } else {
          // Note: can be replaced with 'static_assert(false);' in C++23
          static_assert(always_false_v<T>, "non-exhaustive visitor!");
        }
      },
      singleLineCell._data);
  return os;
}

SimpleTable::Cell::size_type SimpleTable::Cell::width() const {
  const auto maxWidthLineIt = std::ranges::max_element(
      _singleLineCells, [](const auto &lhs, const auto &rhs) { return lhs.width() < rhs.width(); });
  return maxWidthLineIt == _singleLineCells.end() ? size_type{} : maxWidthLineIt->width();
}

void SimpleTable::Cell::print(std::ostream &os, size_type linePos, size_type maxCellWidth) const {
  os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);

  if (linePos < size()) {
    os << _singleLineCells[linePos];
  } else {
    // No value for this line pos for given cell, just print spaces
    os << kEmptyValueChar;
  }

  os << ' ' << kColumnSep;
}

bool SimpleTable::Row::isMultiLine() const noexcept {
  return std::ranges::any_of(_cells, [](const Cell &cell) { return cell.size() > 1U; });
}

void SimpleTable::Row::print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const {
  const auto maxSingleLineCellsIt =
      std::ranges::max_element(_cells, [](const Cell &lhs, const Cell &rhs) { return lhs.size() < rhs.size(); });
  const auto maxNbSingleLineCells = maxSingleLineCellsIt == _cells.end() ? size_type{} : maxSingleLineCellsIt->size();
  for (std::remove_const_t<decltype(maxNbSingleLineCells)> linePos = 0; linePos < maxNbSingleLineCells; ++linePos) {
    os << kColumnSep;

    size_type columnPos{};

    for (const Cell &cell : _cells) {
      cell.print(os, linePos, maxWidthPerColumn[columnPos]);
      ++columnPos;
    }

    os << '\n';
  }
}

SimpleTable::MaxWidthPerColumnVector SimpleTable::computeMaxWidthPerColumn() const {
  // We assume that each row has same number of cells
  const size_type nbColumns = _rows.front().size();
  MaxWidthPerColumnVector res(nbColumns, 0);
  for (const Row &row : _rows) {
    if (row.isDivider()) {
      continue;
    }
    for (size_type columnPos = 0; columnPos < nbColumns; ++columnPos) {
      const Cell &cell = row[columnPos];

      res[columnPos] = std::max(res[columnPos], static_cast<uint16_t>(cell.width()));
    }
  }
  return res;
}

namespace {
auto ComputeLineSep(std::span<const uint16_t> maxWidthPerColumnVector, char cellFiller, char columnSep) {
  const SimpleTable::size_type sumWidths =
      std::accumulate(maxWidthPerColumnVector.begin(), maxWidthPerColumnVector.end(), 0U);

  // 3 as one space before, one space after the field name and column separator. +1 for the first column separator
  const SimpleTable::size_type tableWidth = sumWidths + maxWidthPerColumnVector.size() * 3 + 1;

  SimpleTable::value_type::value_type::value_type::string_type lineSep(tableWidth, cellFiller);

  SimpleTable::size_type curWidth{};
  lineSep[curWidth] = columnSep;
  for (auto maxWidth : maxWidthPerColumnVector) {
    curWidth += maxWidth + 3;
    lineSep[curWidth] = columnSep;
  }

  return lineSep;
}
}  // namespace

std::ostream &operator<<(std::ostream &os, const SimpleTable &table) {
  if (table._rows.empty()) {
    return os;
  }

  const auto maxWidthPerColumnVector = table.computeMaxWidthPerColumn();
  const auto lineSep = ComputeLineSep(maxWidthPerColumnVector, '-', '+');
  const auto multiLineSep = ComputeLineSep(maxWidthPerColumnVector, '~', '|');

  os << lineSep << '\n';

  bool isLastLineSep = false;
  for (SimpleTable::size_type rowPos{}, nbRows = table.size(); rowPos < nbRows; ++rowPos) {
    const SimpleTable::Row &row = table[rowPos];

    if (row.isDivider()) {
      os << lineSep << '\n';
      isLastLineSep = true;
      continue;
    }

    const bool isMultiLine = row.isMultiLine();

    if (isMultiLine && !isLastLineSep) {
      os << multiLineSep << '\n';
    }

    row.print(os, maxWidthPerColumnVector);

    if (rowPos == 0 && nbRows > 1) {
      // header sep
      os << lineSep << '\n';
      isLastLineSep = true;
    } else {
      isLastLineSep = isMultiLine && rowPos + 1 < nbRows && !table[rowPos + 1].isDivider();

      if (isLastLineSep) {
        os << multiLineSep << '\n';
      }
    }
  }

  return os << lineSep;
}
}  // namespace cct