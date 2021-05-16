#pragma once

#include <cassert>
#include <cmath>
#include <iomanip>
#include <ios>
#include <iostream>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>

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
 * VariadicTable<std::string, double, int, std::string> vt("Name", "Weight", "Age", "Brother");
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
  typedef std::tuple<Ts...> DataTuple;

  /**
   * Construct the table with headers
   *
   * @param headers The names of the columns
   * @param static_column_size The size of columns that can't be found automatically
   */
  VariadicTable(std::span<const std::string> headers, std::size_t static_column_size = 0, std::size_t cell_padding = 1)
      : _headers(headers.begin(), headers.end()),
        _num_columns(std::tuple_size<DataTuple>::value),
        _static_column_size(static_column_size),
        _cell_padding(cell_padding) {
    assert(headers.size() == _num_columns);
  }

  VariadicTable(std::initializer_list<std::string> init)
      : VariadicTable(std::span<const std::string>(init.begin(), init.end())) {}

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
    size_columns();

    // Start computing the total width
    // First - we will have _num_columns + 1 "|" characters
    std::size_t total_width = _num_columns + 1;

    // Now add in the size of each colum
    for (const auto &col_size : _column_sizes) {
      total_width += col_size + (2 * _cell_padding);
    }

    if (printHeaders) {
      // Print out the top line
      stream << std::string(total_width, headerLineSep) << std::endl;

      // Print out the headers
      stream << colSep;
      for (std::size_t i = 0; i < _num_columns; i++) {
        // Must find the center of the column
        std::size_t half = _column_sizes[i] / 2;
        half -= _headers[i].size() / 2;

        stream << std::string(_cell_padding, ' ') << std::setw(_column_sizes[i]) << std::left
               << std::string(half, ' ') + _headers[i] << std::string(_cell_padding, ' ') << colSep;
      }

      stream << std::endl;

      // Print out the line below the header
      stream << std::string(total_width, headerLineSep) << std::endl;
    }

    // Now print the rows of the table
    for (const DataTuple &row : _data) {
      stream << colSep;
      print_each(row, stream, colSep);
      stream << std::endl;
    }

    if (printHeaders) {
      // Print out the line below the header
      stream << std::string(total_width, headerLineSep) << std::endl;
    }
  }

  /**
   * Set how to format numbers for each column
   *
   * Note: this is ignored for std::string columns
   *
   * @column_format The format for each column: MUST be the same length as the number of columns.
   */
  void setColumnFormat(const cct::vector<VariadicTableColumnFormat> &column_format) {
    assert(column_format.size() == std::tuple_size<DataTuple>::value);

    _column_format = column_format;
  }

  /**
   * Set how many digits of precision to show for floating point numbers
   *
   * Note: this is ignored for std::string columns
   *
   * @column_format The precision for each column: MUST be the same length as the number of columns.
   */
  void setColumnPrecision(const cct::vector<int> &precision) {
    assert(precision.size() == std::tuple_size<DataTuple>::value);
    _precision = precision;
  }

 protected:
  // Just some handy typedefs for the following two functions
  typedef decltype(&std::right) right_type;
  typedef decltype(&std::left) left_type;
  using SizeVector = cct::vector<std::size_t>;

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
  void print_each(
      TupleType &&, StreamType & /*stream*/, char,
      std::integral_constant<size_t, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>) {}

  /**
   * This gets called on each item
   */
  template <std::size_t I, typename TupleType, typename StreamType,
            typename = typename std::enable_if<
                I != std::tuple_size<typename std::remove_reference<TupleType>::type>::value>::type>
  void print_each(TupleType &&t, StreamType &stream, char colSep, std::integral_constant<size_t, I>) {
    auto &val = std::get<I>(t);

    // Set the precision
    if (!_precision.empty()) {
      assert(_precision.size() == std::tuple_size<typename std::remove_reference<TupleType>::type>::value);

      stream << std::setprecision(_precision[I]);
    }

    // Set the format
    if (!_column_format.empty()) {
      assert(_column_format.size() == std::tuple_size<typename std::remove_reference<TupleType>::type>::value);

      if (_column_format[I] == VariadicTableColumnFormat::SCIENTIFIC) stream << std::scientific;

      if (_column_format[I] == VariadicTableColumnFormat::FIXED) stream << std::fixed;

      if (_column_format[I] == VariadicTableColumnFormat::PERCENT) stream << std::fixed << std::setprecision(2);
    }

    stream << std::string(_cell_padding, ' ') << std::setw(_column_sizes[I]) << justify<decltype(val)>(0) << val
           << std::string(_cell_padding, ' ') << colSep;

    // Unset the format
    if (!_column_format.empty()) {
      // Because "stream << std::defaultfloat;" won't compile with old GCC or Clang
      stream.unsetf(std::ios_base::floatfield);
    }

    // Recursive call to print the next item
    print_each(std::forward<TupleType>(t), stream, colSep, std::integral_constant<size_t, I + 1>());
  }

  /**
   * his is what gets called first
   */
  template <typename TupleType, typename StreamType>
  void print_each(TupleType &&t, StreamType &stream, char colSep) {
    print_each(std::forward<TupleType>(t), stream, colSep, std::integral_constant<size_t, 0>());
  }

  /**
   * Try to find the size the column will take up
   *
   * If the datatype has a size() member... let's call it
   */
  template <class T>
  size_t sizeOfData(const T &data, decltype(((T *)nullptr)->size()) * /*dummy*/ = nullptr) {
    return data.size();
  }

  /**
   * Try to find the size the column will take up
   *
   * If the datatype is an integer - let's get it's length
   */
  template <class T>
  size_t sizeOfData(const T &data, typename std::enable_if<std::is_integral<T>::value>::type * /*dummy*/ = nullptr) {
    if (data == 0) return 1;

    return std::log10(data) + 1;
  }

  /**
   * If it doesn't... let's just use a statically set size
   */
  size_t sizeOfData(...) { return _static_column_size; }

  /**
   * These three functions iterate over the Tuple, find the printed size of each element and set it
   * in a vector
   */

  /**
   * End the recursion
   */
  template <typename TupleType>
  void size_each(
      TupleType &&, SizeVector & /*sizes*/,
      std::integral_constant<size_t, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>) {}

  /**
   * Recursively called for each element
   */
  template <std::size_t I, typename TupleType,
            typename = typename std::enable_if<
                I != std::tuple_size<typename std::remove_reference<TupleType>::type>::value>::type>
  void size_each(TupleType &&t, SizeVector &sizes, std::integral_constant<size_t, I>) {
    sizes[I] = sizeOfData(std::get<I>(t));

    // Override for Percent
    if (!_column_format.empty())
      if (_column_format[I] == VariadicTableColumnFormat::PERCENT) sizes[I] = 6;  // 100.00

    // Continue the recursion
    size_each(std::forward<TupleType>(t), sizes, std::integral_constant<size_t, I + 1>());
  }

  /**
   * The function that is actually called that starts the recursion
   */
  template <typename TupleType>
  void size_each(TupleType &&t, SizeVector &sizes) {
    size_each(std::forward<TupleType>(t), sizes, std::integral_constant<size_t, 0>());
  }

  /**
   * Finds the size each column should be and set it in _column_sizes
   */
  void size_columns() {
    _column_sizes.resize(_num_columns);

    // Temporary for querying each row
    SizeVector column_sizes(static_cast<SizeVector::size_type>(_num_columns));

    // Start with the size of the headers
    for (std::size_t i = 0; i < _num_columns; i++) _column_sizes[i] = _headers[i].size();

    // Grab the size of each entry of each row and see if it's bigger
    for (const DataTuple &row : _data) {
      size_each(row, column_sizes);

      for (std::size_t i = 0; i < _num_columns; i++) _column_sizes[i] = std::max(_column_sizes[i], column_sizes[i]);
    }
  }

  /// The column headers
  cct::vector<std::string> _headers;

  /// Number of columns in the table
  std::size_t _num_columns;

  /// Size of columns that we can't get the size of
  std::size_t _static_column_size;

  /// Size of the cell padding
  std::size_t _cell_padding;

  /// The actual data
  cct::vector<DataTuple> _data;

  /// Holds the printable width of each column
  SizeVector _column_sizes;

  /// Column Format
  cct::vector<VariadicTableColumnFormat> _column_format;

  /// Precision For each column
  cct::vector<int> _precision;
};
}  // namespace cct