#include "simpletable.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <numeric>

#include "cct_smallvector.hpp"
#include "mathhelpers.hpp"
#include "unreachable.hpp"

namespace cct {

/// A divider row is represented as an empty row.
/// It will be treated specially in the print
const SimpleTable::Row SimpleTable::Row::kDivider;

namespace {
constexpr char kColumnSep = '|';
constexpr char kLineSep = '-';

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

void SimpleTable::print(std::ostream &os) const {
  if (_rows.empty()) {
    return;
  }
  // We assume that each row has same number of cells, no silly checks here
  const size_type nbColumns = _rows.front().size();
  SmallVector<uint16_t, 8> maxWidthPerColumn(nbColumns, 0);
  for (const Row &row : _rows) {
    if (!row.isDivider()) {
      for (size_type columnPos = 0; columnPos < nbColumns; ++columnPos) {
        maxWidthPerColumn[columnPos] =
            std::max(maxWidthPerColumn[columnPos], static_cast<uint16_t>(row[columnPos].size()));
      }
    }
  }
  const size_type sumWidths = std::accumulate(maxWidthPerColumn.begin(), maxWidthPerColumn.end(), 0U);
  const size_type maxTableWidth = sumWidths + nbColumns * 3 + 1;
  const Cell::string_type lineSep(maxTableWidth, kLineSep);

  os << lineSep << std::endl;

  bool printHeader = _rows.size() > 1U;
  for (const Row &row : _rows) {
    if (row.isDivider()) {
      os << lineSep << std::endl;
    } else {
      row.print(os, maxWidthPerColumn);
    }
    if (printHeader) {
      os << lineSep << std::endl;
      printHeader = false;
    }
  }

  os << lineSep;
}
}  // namespace cct