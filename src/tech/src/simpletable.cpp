#include "simpletable.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <numeric>
#include <ostream>
#include <span>
#include <string_view>
#include <variant>

#include "mathhelpers.hpp"
#include "unreachable.hpp"

namespace cct {

/// A divider row is represented as an empty row.
/// It will be treated specially in the print
const SimpleTable::Row SimpleTable::Row::kDivider;

namespace {
constexpr char kColumnSep = '|';

constexpr std::string_view kBoolValueTrue = "yes";
constexpr std::string_view kBoolValueFalse = "no";

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
}  // namespace

SimpleTable::size_type SimpleTable::Cell::size() const noexcept {
  return std::visit(
      [](auto &&v) -> size_type {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, string_type> || std::is_same_v<T, std::string_view>) {
          return v.size();
        } else if constexpr (std::is_same_v<T, bool>) {
          return v ? kBoolValueTrue.size() : kBoolValueFalse.size();
        } else if constexpr (std::is_integral_v<T>) {
          return nchars(v);
        } else {
          unreachable();
        }
      },
      _data);
}

void SimpleTable::Cell::print(std::ostream &os, size_type maxCellWidth) const {
  os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);

  std::visit(
      [&os](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          os << (v ? kBoolValueTrue : kBoolValueFalse);
        } else if constexpr (std::is_same_v<T, string_type> || std::is_same_v<T, std::string_view> ||
                             std::is_integral_v<T>) {
          os << v;
        } else {
          unreachable();
        }
      },
      _data);

  os << ' ' << kColumnSep;
}

void SimpleTable::Row::print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const {
  os << kColumnSep;
  size_type columnPos = 0;
  for (const Cell &cell : _cells) {
    cell.print(os, maxWidthPerColumn[columnPos++]);
  }
  os << '\n';
}

SimpleTable::MaxWidthPerColumnVector SimpleTable::computeMaxWidthPerColumn() const {
  // We assume that each row has same number of cells, no silly checks here
  const size_type nbColumns = _rows.front().size();
  MaxWidthPerColumnVector res(nbColumns, 0);
  for (const Row &row : _rows) {
    if (!row.isDivider()) {
      for (size_type columnPos = 0; columnPos < nbColumns; ++columnPos) {
        res[columnPos] = std::max(res[columnPos], static_cast<uint16_t>(row[columnPos].size()));
      }
    }
  }
  return res;
}

SimpleTable::Cell::string_type SimpleTable::ComputeLineSep(std::span<const uint16_t> maxWidthPerColumnVector) {
  const size_type sumWidths = std::accumulate(maxWidthPerColumnVector.begin(), maxWidthPerColumnVector.end(), 0U);

  // 3 as one space before, one space after the field name and column separator. +1 for the first column separator
  const size_type tableWidth = sumWidths + maxWidthPerColumnVector.size() * 3 + 1;
  Cell::string_type lineSep(tableWidth, '-');

  size_type curWidth = 0;
  lineSep[curWidth] = '+';
  for (auto maxWidth : maxWidthPerColumnVector) {
    curWidth += maxWidth + 3;
    lineSep[curWidth] = '+';
  }

  return lineSep;
}

std::ostream &operator<<(std::ostream &os, const SimpleTable &table) {
  if (table._rows.empty()) {
    return os;
  }
  const auto maxWidthPerColumnVector = table.computeMaxWidthPerColumn();
  const auto lineSep = SimpleTable::ComputeLineSep(maxWidthPerColumnVector);

  os << lineSep << '\n';

  bool printHeader = table._rows.size() > 1U;
  for (const auto &row : table._rows) {
    if (row.isDivider()) {
      os << lineSep << '\n';
    } else {
      row.print(os, maxWidthPerColumnVector);
    }
    if (printHeader) {
      os << lineSep << '\n';
      printHeader = false;
    }
  }

  return os << lineSep;
}
}  // namespace cct