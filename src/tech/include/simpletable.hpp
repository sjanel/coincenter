#pragma once

#include <cstdint>
#include <ios>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct {
using size_type = uint32_t;

/// Simple, lightweight and fast table with dynamic number of columns.
/// No checks are made about the number of columns for each Row, it's up to client's responsibility to make sure they
/// match.
class SimpleTable {
 public:
  class Row;

  /// Cell in a SimpleTable.
  /// Can currently hold only 3 types of values: a string, a string_view or an integral type.
  /// TODO: add support of 'double' if needed
  class Cell {
   public:
    using IntegralType = int64_t;
    using value_type = std::variant<string, std::string_view, IntegralType>;

    explicit Cell(std::string_view v) : _data(v) {}

    explicit Cell(const char *v) : _data(std::string_view(v)) {}

    explicit Cell(const string &v) : _data(v) {}

    explicit Cell(string &&v) : _data(std::move(v)) {}

    explicit Cell(IntegralType v) : _data(v) {}

    size_type size() const noexcept;

   private:
    friend class Row;

    void print(std::ostream &os, size_type maxCellWidth) const;

    value_type _data;
  };

  /// Row in a SimpleTable.
  class Row {
   public:
    using value_type = Cell;
    using const_iterator = vector<value_type>::const_iterator;

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

    void print(std::ostream &os, std::span<const uint16_t> maxWidthPerColumn) const;

    vector<value_type> _cells;
  };

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

  void reserve(size_type s) { _rows.reserve(s); }

  void print(std::ostream &os = std::cout) const;

 private:
  vector<Row> _rows;
};
}  // namespace cct