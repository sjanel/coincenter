#pragma once

namespace cct {

/// Simple utility class that may iterate in both directions.
class ContinuousIterator {
 public:
  ContinuousIterator(int from, int to) : _to(to), _curr(from), _incr(to < from ? -1 : 1) {}

  bool hasNext() const { return _curr != _to + _incr; }

  auto next() {
    _curr += _incr;
    return _curr - _incr;
  }

 private:
  int _to;
  int _curr;
  int _incr;
};

}  // namespace cct
