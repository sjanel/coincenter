#pragma once

#include <filesystem>
#include <iostream>
#include <span>
#include <utility>

#include "cct_vector.hpp"
#include "coincenteroptions.hpp"
#include "commandlineoptionsparseriterator.hpp"

namespace cct {
template <class ParserType>
auto ParseOptions(ParserType &parser, int argc, const char *argv[]) {
  auto programName = std::filesystem::path(argv[0]).filename().string();

  std::span<const char *> allArguments(argv, argc);

  // skip first argument which is program name
  CommandLineOptionsParserIterator parserIt(parser, allArguments.last(allArguments.size() - 1U));

  using OptValueType = ParserType::value_type;
  OptValueType globalOptions;

  vector<OptValueType> parsedOptions;

  // Support for command line multiple commands. Only full name flags are supported for multi command line commands.
  while (parserIt.hasNext()) {
    auto groupedArguments = parserIt.next();

    auto groupParsedOptions = parser.parse(groupedArguments);
    globalOptions.mergeGlobalWith(groupParsedOptions);

    if (groupedArguments.empty()) {
      groupParsedOptions.help = true;
    }
    if (groupParsedOptions.help) {
      parser.displayHelp(programName, std::cout);
    } else if (groupParsedOptions.version) {
      CoincenterCmdLineOptions::PrintVersion(programName, std::cout);
    } else {
      // Only store commands if they are not 'help' nor 'version'
      parsedOptions.push_back(std::move(groupParsedOptions));
    }
  }

  // Apply global options to all parsed options containing commands
  for (auto &groupParsedOptions : parsedOptions) {
    groupParsedOptions.mergeGlobalWith(globalOptions);
  }

  return parsedOptions;
}
}  // namespace cct