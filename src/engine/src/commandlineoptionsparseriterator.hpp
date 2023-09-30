#pragma once

#include <span>
#include <string_view>

#include "commandlineoptionsparser.hpp"

namespace cct {

template <class OptValueType>
class CommandLineOptionsParserIterator {
 public:
  CommandLineOptionsParserIterator(const CommandLineOptionsParser<OptValueType>& parser,
                                   std::span<const char*> allArguments)
      : _parser(parser),
        _allArguments(allArguments),
        _begIt(_allArguments.begin()),
        _endIt(getNextGroupedEndIt(_begIt)) {}

  /**
   * @brief Tells whether this iterator has at least one more element.
   */
  [[nodiscard]] bool hasNext() const { return !_hasReturnedAtLeastOneSpan || _begIt != _endIt; }

  /**
   * @brief Get next grouped arguments that should be treated together.
   *        hasNext needs to return true prior to the call to this method
   */
  std::span<const char*> next() {
    std::span<const char*> ret(_begIt, _endIt);
    _begIt = _endIt;
    _endIt = getNextGroupedEndIt(_endIt);
    _hasReturnedAtLeastOneSpan = true;
    return ret;
  }

 private:
  using ConstIt = std::span<const char*>::iterator;

  [[nodiscard]] ConstIt getNextGroupedEndIt(ConstIt searchFromIt) const {
    if (searchFromIt == _allArguments.end()) {
      return _allArguments.end();
    }
    while (++searchFromIt != _allArguments.end()) {
      std::string_view optStr(*searchFromIt);
      for (const auto& [cmdLineOption, _] : _parser._opts) {
        std::string_view cmdLineOptionFullName = cmdLineOption.fullName();
        if (cmdLineOptionFullName[0] != '-' && cmdLineOptionFullName == optStr) {
          return searchFromIt;
        }
      }
    }

    return searchFromIt;
  }

  const CommandLineOptionsParser<OptValueType>& _parser;
  std::span<const char*> _allArguments;
  ConstIt _begIt;
  ConstIt _endIt;
  bool _hasReturnedAtLeastOneSpan = false;
};
}  // namespace cct