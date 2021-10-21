#pragma once

#include <cassert>
#include <cmath>
#include <iomanip>
#include <ios>
#include <iostream>
#include <span>
#include <tuple>
#include <type_traits>

#include "cct_mathhelpers.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct {

/**
 * Used to specify the column format
 */
enum class VariadicTableColumnFormat { AUTO, SCIENTIFIC, FIXED, PERCENT };

/**
 * A class for "pretty printing" a table of data.
 *
 * Requires C++11 (and nothing more)
 *
 * It's templated on the types that will be in each column
 * (all values in a column must have the same type)
 *
 * For instance, to use it with data that looks like:  "Fred", 193.4, 35, "Sam"
 * with header names: "Name", "Weight", "Age", "Brother"
 *
 * You would invoke the table like so:
 * VariadicTable<string, double, int, string> vt("Name", "Weight", "Age", "Brother");
 *
 * Then add the data to the table:
 * vt.addRow("Fred", 193.4, 35, "Sam");
 *
 * And finally print it:
 * vt.print();
 */
template <class... Ts>
class VariadicTable {
 public:
  /// The type stored for each row
  using DataTuple = std::tuple<Ts...>;

 private:
  /// Number of columns in the table
  static constexpr uint32_t kNbColumns = std::tuple_size<DataTuple>::value;

 public:
  /**
   * Construct the table with headers
   *
   * @param headers The names of the columns
   */
  VariadicTable(std::span<const string> headers, uint32_t staticColumnSize = 0, uint32_t cellPadding = 1)
      : _headers(headers.begin(), headers.end()), _staticColumnSize(staticColumnSize), _cellPadding(cellPadding) {
    assert(headers.size() == kNbColumns);
  }

  VariadicTable(std::initializer_list<string> init)
      : VariadicTable(std::span<const string>(init.begin(), init.end())) {}

  /**
   * Add a row of data
   *
   * Easiest to use like:
   * table.addRow({data1, data2, data3});
   *
   * @param data A Tuple of data to add
   */
  void addRow(Ts... entries) { _data.emplace_back(std::make_tuple(entries...)); }

  void print() { print(std::cout); }

  /**
   * Pretty print the table of data
   */
  template <typename StreamType>
  void print(StreamType &stream, char colSep = '|', char headerLineSep = '-', bool printHeaders = true) {
    sizeColumns();

    // Start computing the total width
    // First - we will have kNbColumns + 1 "|" characters
    auto totalWidth = kNbColumns + 1;

    // Now add in the size of each colum
    for (auto colSize : _columnSizes) {
      totalWidth += colSize + 2 * _cellPadding;
    }

    const string headerLine(totalWidth, headerLineSep);

    if (printHeaders) {
      // Print out the top line
      stream << headerLine << std::endl;

      // Print out the headers
      stream << colSep;
      for (uint32_t i = 0; i < kNbColumns; ++i) {
        // Must find the center of the column
        std::size_t half = _columnSizes[i] / 2;
        half -= _headers[i].size() / 2;

        stream << string(_cellPadding, ' ') << std::setw(_columnSizes[i]) << std::left
               << string(half, ' ') + _headers[i] << string(_cellPadding, ' ') << colSep;
      }

      stream << std::endl;

      // Print out the line below the header
      stream << headerLine << std::endl;
    }

    // Now print the rows of the table
    for (const DataTuple &row : _data) {
      stream << colSep;
      printEach(row, stream, colSep);
      stream << std::endl;
    }

    if (printHeaders) {
      // Print out the line below the header
      stream << headerLine << std::endl;
    }
  }

  /**
   * Set how to format numbers for each column
   *
   * Note: this is ignored for string columns
   *
   * @column_format The format for each column: MUST be the same length as the number of columns.
   */
  void setColumnFormat(const vector<VariadicTableColumnFormat> &column_format) {
    assert(column_format.size() == std::tuple_size<DataTuple>::value);
    _columnFormat = column_format;
  }

  /**
   * Set how many digits of precision to show for floating point numbers
   *
   * Note: this is ignored for string columns
   *
   * @column_format The precision for each column: MUST be the same length as the number of columns.
   */
  void setColumnPrecision(const vector<int> &precision) {
    assert(precision.size() == std::tuple_size<DataTuple>::value);
    _precision = precision;
  }

  void setColumnPrecision(vector<int> &&precision) {
    assert(precision.size() == std::tuple_size<DataTuple>::value);
    _precision = std::move(precision);
  }

 protected:
  // Just some handy types for the following two functions
  using right_type = decltype(&std::right);
  using left_type = decltype(&std::left);

  using SizeVector = vector<uint32_t>;
  using size_type = SizeVector::size_type;

  // Attempts to figure out the correct justification for the data
  // If it's a floating point value
  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<typename std::remove_reference<T>::type>::value>::type>
  static right_type justify(int /*firstchoice*/) {
    return std::right;
  }

  // Otherwise
  template <typename T>
  static left_type justify(long /*secondchoice*/) {
    return std::left;
  }

  /**
   * These three functions print out each item in a Tuple into the table
   *
   * Original Idea From From https://stackoverflow.com/a/26908596
   *
   * BTW: This would all be a lot easier with generic lambdas
   * there would only need to be one of this sequence and then
   * you could pass in a generic lambda.  Unfortunately, that's C++14
   */

  /**
   *  This ends the recursion
   */
  template <typename TupleType, typename StreamType>
  void printEach(
      TupleType &&, StreamType & /*stream*/, char,
      std::integral_constant<size_t, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>) const {}

  /**
   * This gets called on each item
   */
  template <std::size_t I, typename TupleType, typename StreamType,
            typename = typename std::enable_if<
                I != std::tuple_size<typename std::remove_reference<TupleType>::type>::value>::type>
  void printEach(TupleType &&t, StreamType &stream, char colSep, std::integral_constant<size_t, I>) const {
    // Set the precision
    if (!_precision.empty()) {
      assert(_precision.size() == std::tuple_size<typename std::remove_reference<TupleType>::type>::value);

      stream << std::setprecision(_precision[I]);
    }

    // Set the format
    if (!_columnFormat.empty()) {
      assert(_columnFormat.size() == std::tuple_size<typename std::remove_reference<TupleType>::type>::value);

      switch (_columnFormat[I]) {
        case VariadicTableColumnFormat::AUTO:
          break;
        case VariadicTableColumnFormat::SCIENTIFIC:
          stream << std::scientific;
          break;
        case VariadicTableColumnFormat::FIXED:
          stream << std::fixed;
          break;
        case VariadicTableColumnFormat::PERCENT:
          stream << std::fixed << std::setprecision(2);
          break;
      }
    }

    const auto &val = std::get<I>(t);
    stream << string(_cellPadding, ' ') << std::setw(_columnSizes[I]) << justify<decltype(val)>(0) << val
           << string(_cellPadding, ' ') << colSep;

    // Unset the format
    if (!_columnFormat.empty()) {
      stream << std::defaultfloat;
    }

    // Recursive call to print the next item
    printEach(std::forward<TupleType>(t), stream, colSep, std::integral_constant<size_t, I + 1>());
  }

  /**
   * his is what gets called first
   */
  template <typename TupleType, typename StreamType>
  void printEach(TupleType &&t, StreamType &stream, char colSep) const {
    printEach(std::forward<TupleType>(t), stream, colSep, std::integral_constant<size_t, 0>());
  }

  /**
   * Try to find the size the column will take up
   *
   * If the datatype has a size() member... let's call it
   */
  template <class T>
  uint32_t sizeOfData(const T &data, decltype(((T *)nullptr)->size()) * = 0) const {
    return static_cast<uint32_t>(data.size());
  }

  /**
   * Try to find the size the column will take up
   *
   * If the datatype is an integer - let's get it's length
   */
  template <class T>
  uint32_t sizeOfData(const T &data, typename std::enable_if<std::is_integral<T>::value>::type * = 0) const {
    return ndigits(data);
  }

  /**
   * If it doesn't... let's just use a statically set size
   */
  size_t sizeOfData(...) const { return _staticColumnSize; }

  /**
   * These three functions iterate over the Tuple, find the printed size of each element and set it
   * in a vector
   */

  /**
   * End the recursion
   */
  template <typename TupleType>
  void sizeEach(
      TupleType &&, SizeVector &,
      std::integral_constant<uint32_t, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>) const {
  }

  /**
   * Recursively called for each element
   */
  template <uint32_t I, typename TupleType,
            typename = typename std::enable_if<
                I != std::tuple_size<typename std::remove_reference<TupleType>::type>::value>::type>
  void sizeEach(TupleType &&t, SizeVector &sizes, std::integral_constant<uint32_t, I>) const {
    sizes[I] = sizeOfData(std::get<I>(t));

    // Override for Percent
    if (!_columnFormat.empty()) {
      if (_columnFormat[I] == VariadicTableColumnFormat::PERCENT) {
        sizes[I] = 6;  // 100.00
      }
    }

    // Continue the recursion
    sizeEach(std::forward<TupleType>(t), sizes, std::integral_constant<uint32_t, I + 1>());
  }

  /**
   * The function that is actually called that starts the recursion
   */
  template <typename TupleType>
  void sizeEach(TupleType &&t, SizeVector &sizes) const {
    sizeEach(std::forward<TupleType>(t), sizes, std::integral_constant<uint32_t, 0>());
  }

  /**
   * Finds the size each column should be and set it in _columnSizes
   */
  void sizeColumns() {
    _columnSizes.resize(kNbColumns);

    // Temporary for querying each row
    SizeVector columnSizes(kNbColumns);

    // Start with the size of the headers
    for (uint32_t i = 0; i < kNbColumns; i++) {
      _columnSizes[i] = static_cast<uint32_t>(_headers[i].size());
    }

    // Grab the size of each entry of each row and see if it's bigger
    for (const DataTuple &row : _data) {
      sizeEach(row, columnSizes);

      for (uint32_t i = 0; i < kNbColumns; i++) {
        _columnSizes[i] = std::max(_columnSizes[i], columnSizes[i]);
      }
    }
  }

  /// The column headers
  vector<string> _headers;

  /// Size of columns that we can't get the size of
  uint32_t _staticColumnSize;

  /// The actual data
  vector<DataTuple> _data;

  /// Holds the printable width of each column
  SizeVector _columnSizes;

  /// Column Format
  vector<VariadicTableColumnFormat> _columnFormat;

  /// Precision For each column
  vector<int> _precision;

  /// Size of the cell padding
  uint32_t _cellPadding;
};
}  // namespace cct
