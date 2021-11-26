#include "simpletable.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <numeric>

#include "cct_smallvector.hpp"
#include "mathhelpers.hpp"

namespace cct {
namespace {
constexpr char kColumnSep = '|';
constexpr char kLineSep = '-';

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
}  // namespace

size_type SimpleTable::Cell::size() const noexcept {
  switch (_data.index()) {
    case 0:
      return static_cast<size_type>(std::get<string>(_data).size());
    case 1:
      return static_cast<size_type>(std::get<std::string_view>(_data).size());
    case 2:
      return static_cast<size_type>(ndigits(std::get<IntegralType>(_data)));
    default:
      return 0U;
  }
}

void SimpleTable::Cell::print(std::ostream &os, size_type maxCellWidth) const {
  os << ' ' << Align(AlignTo::kLeft) << std::setw(maxCellWidth);
  switch (_data.index()) {
    case 0:
      os << std::get<string>(_data);
      break;
    case 1:
      os << std::get<std::string_view>(_data);
      break;
    case 2: {
      static constexpr int kBufSize = ndigits(std::numeric_limits<IntegralType>::max()) + 1;  // +1 for '-'
      std::array<char, kBufSize> buf;

      if (auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + kBufSize, std::get<IntegralType>(_data));
          ec == std::errc()) {
        os << std::string_view(buf.data(), ptr);
      }
      break;
    }
  }
  os << ' ' << kColumnSep;
}

void SimpleTable::Row::print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const {
  os << kColumnSep;
  size_type columnPos = 0;
  for (const Cell &c : _cells) {
    c.print(os, maxWidthPerColumn[columnPos++]);
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
  for (const Row &r : _rows) {
    for (size_type columnPos = 0; columnPos < nbColumns; ++columnPos) {
      maxWidthPerColumn[columnPos] = std::max(maxWidthPerColumn[columnPos], static_cast<uint16_t>(r[columnPos].size()));
    }
  }
  const size_type sumWidths = std::accumulate(maxWidthPerColumn.begin(), maxWidthPerColumn.end(), 0U);
  const size_type maxTableWidth = sumWidths + nbColumns * 3 + 1;
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
}  // namespace cct