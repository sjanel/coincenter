#pragma once

#include <cstdint>
#include <string_view>

#include "time-window.hpp"

namespace cct {

class ReplayOptions {
 public:
  enum class ReplayMode : int8_t { kValidateOnly, kCheckedLaunchAlgorithm, kUncheckedLaunchAlgorithm };

  ReplayOptions() noexcept = default;

  /// Algorithm names should be comma separated. Empty string will match all.
  ReplayOptions(TimeWindow timeWindow, std::string_view algorithmNames, ReplayMode replayMode);

  TimeWindow timeWindow() const { return _timeWindow; }

  std::string_view algorithmNames() const;

  ReplayMode replayMode() const { return _replayMode; }

  bool operator==(const ReplayOptions &) const noexcept = default;

 private:
  TimeWindow _timeWindow;
  std::string_view _algorithmNames;
  ReplayMode _replayMode;
};

}  // namespace cct