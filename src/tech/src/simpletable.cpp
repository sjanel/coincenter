#include "simpletable.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <numeric>

#include "mathhelpers.hpp"
#include "unreachable.hpp"

namespace cct {

/// A divider row is represented as an empty row.
/// It will be treated specially in the print
const SimpleTable::Row SimpleTable::Row::kDivider;

namespace {
constexpr char kColumnSep = '|';

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
  switch (_data.index()) {
    case 0:
      return static_cast<size_type>(std::get<string_type>(_data).size());
    case 1:
      return static_cast<size_type>(std::get<std::string_view>(_data).size());
    case 2:
      return static_cast<size_type>(nchars(std::get<IntegralType>(_data)));
    default:
      unreachable();
  }
}

void SimpleTable::Cell::print(std::ostream &os, size_type maxCellWidth) const {
  os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);
  switch (_data.index()) {
    case 0:
      os << std::get<string_type>(_data);
      break;
    case 1:
      os << std::get<std::string_view>(_data);
      break;
    case 2:
      os << std::get<IntegralType>(_data);
      break;
    default:
      unreachable();
  }
  os << ' ' << kColumnSep;
}

void SimpleTable::Row::print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const {
  os << kColumnSep;
  size_type columnPos = 0;
  for (const Cell &cell : _cells) {
    cell.print(os, maxWidthPerColumn[columnPos++]);
  }
  os << std::endl;
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

SimpleTable::Cell::string_type SimpleTable::computeLineSep(std::span<const uint16_t> maxWidthPerColumnVector) const {
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

std::ostream &operator<<(std::ostream &os, const SimpleTable &t) {
  if (t._rows.empty()) {
    return os;
  }
  const auto maxWidthPerColumnVector = t.computeMaxWidthPerColumn();
  const auto lineSep = t.computeLineSep(maxWidthPerColumnVector);

  os << lineSep << std::endl;

  bool printHeader = t._rows.size() > 1U;
  for (const auto &row : t._rows) {
    if (row.isDivider()) {
      os << lineSep << std::endl;
    } else {
      row.print(os, maxWidthPerColumnVector);
    }
    if (printHeader) {
      os << lineSep << std::endl;
      printHeader = false;
    }
  }

  return os << lineSep;
}
}  // namespace cct