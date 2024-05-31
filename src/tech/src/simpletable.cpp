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

#include "cct_smallvector.hpp"
#include "nchars.hpp"

namespace cct {

namespace table {

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

CellLine::size_type CellLine::width() const {
  return std::visit(
      [](auto &&val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, string_type> || std::is_same_v<T, std::string_view>) {
          return static_cast<size_type>(val.length());
        } else if constexpr (std::is_same_v<T, bool>) {
          return static_cast<size_type>(val ? kBoolValueTrue.length() : kBoolValueFalse.length());
        } else if constexpr (std::is_integral_v<T>) {
          return static_cast<size_type>(nchars(val));
        } else {
          // Note: can be replaced with 'static_assert(false);' in C++23
          static_assert(always_false_v<T>, "non-exhaustive visitor!");
        }
      },
      _data);
}

std::ostream &operator<<(std::ostream &os, const CellLine &singleLineCell) {
  std::visit(
      [&os](auto &&val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, bool>) {
          os << (val ? kBoolValueTrue : kBoolValueFalse);
        } else if constexpr (std::is_same_v<T, CellLine::string_type> || std::is_same_v<T, std::string_view> ||
                             std::is_integral_v<T>) {
          os << val;
        } else {
          // Note: can be replaced with 'static_assert(false);' in C++23
          static_assert(always_false_v<T>, "non-exhaustive visitor!");
        }
      },
      singleLineCell._data);
  return os;
}

Cell::size_type Cell::width() const {
  const auto maxWidthLineIt = std::ranges::max_element(
      _singleLineCells, [](const auto &lhs, const auto &rhs) { return lhs.width() < rhs.width(); });
  return maxWidthLineIt == _singleLineCells.end() ? size_type{} : maxWidthLineIt->width();
}

namespace {
bool IsMultiLine(const Row &row) {
  return std::ranges::any_of(row, [](const auto &cell) { return cell.size() > 1U; });
}

void PrintCell(std::ostream &os, const Cell &cell, Cell::size_type linePos, Cell::size_type maxCellWidth) {
  os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);

  if (linePos < cell.size()) {
    os << cell[linePos];
  } else {
    // No value for this line pos for given cell, just print spaces
    os << kEmptyValueChar;
  }

  os << ' ' << kColumnSep;
}

void PrintRow(std::ostream &os, const Row &row, std::span<const uint16_t> maxWidthPerColumn) {
  const auto maxSingleLineCellsIt =
      std::ranges::max_element(row, [](const auto &lhs, const auto &rhs) { return lhs.size() < rhs.size(); });
  const auto maxNbSingleLineCells = maxSingleLineCellsIt == row.end() ? 0 : maxSingleLineCellsIt->size();
  using size_type = std::remove_const_t<decltype(maxNbSingleLineCells)>;
  for (size_type linePos = 0; linePos < maxNbSingleLineCells; ++linePos) {
    os << kColumnSep;

    size_type columnPos{};

    for (const auto &cell : row) {
      PrintCell(os, cell, linePos, maxWidthPerColumn[columnPos]);
      ++columnPos;
    }

    os << '\n';
  }
}
}  // namespace

}  // namespace table

namespace {
auto ComputeMaxWidthPerColumn(const SimpleTable &table) {
  const auto nbColumns = table.front().size();
  SmallVector<uint16_t, 16> res(nbColumns, 0);
  for (const auto &row : table) {
    if (row.empty()) {
      continue;
    }

    for (std::remove_const_t<decltype(nbColumns)> columnPos{}; columnPos < nbColumns; ++columnPos) {
      res[columnPos] = std::max(res[columnPos], static_cast<uint16_t>(row[columnPos].width()));
    }
  }
  return res;
}

auto ComputeLineSep(std::span<const uint16_t> maxWidthPerColumnVector, char cellFiller, char columnSep) {
  using size_type = SimpleTable::size_type;
  using string_type = SimpleTable::value_type::value_type::value_type::string_type;

  const auto sumWidths = std::accumulate(maxWidthPerColumnVector.begin(), maxWidthPerColumnVector.end(), size_type{});

  // 3 as one space before, one space after the field name and column separator. +1 for the first column separator
  const auto tableWidth = sumWidths + (static_cast<size_type>(maxWidthPerColumnVector.size()) * 3) + 1;

  string_type lineSep(tableWidth, cellFiller);

  size_type curWidth{};
  lineSep[curWidth] = columnSep;
  for (auto maxWidth : maxWidthPerColumnVector) {
    curWidth += maxWidth + 3;
    lineSep[curWidth] = columnSep;
  }

  return lineSep;
}
}  // namespace

std::ostream &operator<<(std::ostream &os, const SimpleTable &table) {
  if (table.empty()) {
    return os;
  }

  const auto maxWidthPerColumnVector = ComputeMaxWidthPerColumn(table);
  const auto lineSep = ComputeLineSep(maxWidthPerColumnVector, '-', '+');
  const auto multiLineSep = ComputeLineSep(maxWidthPerColumnVector, '~', '|');

  os << lineSep << '\n';

  bool isLastLineSep = false;
  for (SimpleTable::size_type rowPos{}, nbRows = table.size(); rowPos < nbRows; ++rowPos) {
    const auto &row = table[rowPos];

    if (row.empty()) {
      os << lineSep << '\n';
      isLastLineSep = true;
      continue;
    }

    const bool isMultiLine = table::IsMultiLine(row);

    if (isMultiLine && !isLastLineSep) {
      os << multiLineSep << '\n';
    }

    table::PrintRow(os, row, maxWidthPerColumnVector);

    if (rowPos == 0 && nbRows > 1) {
      // header sep
      os << lineSep << '\n';
      isLastLineSep = true;
    } else {
      isLastLineSep = isMultiLine && rowPos + 1 < nbRows && !table[rowPos + 1].empty();

      if (isLastLineSep) {
        os << multiLineSep << '\n';
      }
    }
  }

  return os << lineSep;
}
}  // namespace cct