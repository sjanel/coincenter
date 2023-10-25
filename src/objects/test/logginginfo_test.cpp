#include "logginginfo.hpp"

#include <gtest/gtest.h>

#include <utility>

#include "cct_const.hpp"
#include "cct_log.hpp"
#include "generalconfigdefault.hpp"

namespace cct {
TEST(LoggingInfo, SimpleConstructor) {
  LoggingInfo loggingInfo1(LoggingInfo::WithLoggersCreation::kYes);

  log::info("test1");

  LoggingInfo loggingInfo2(LoggingInfo::WithLoggersCreation::kNo);

  log::info("test2");
}

TEST(LoggingInfo, ConstructorFromJson) {
  LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kYes, kDefaultDataDir, GeneralConfigDefault::Prod()["log"]);

  log::info("test");
}

TEST(LoggingInfo, ReentrantTest) {
  {
    LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kYes);

    log::info("test1");
  }

  {
    LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kYes);

    log::info("test2");
  }
}

TEST(LoggingInfo, MoveConstructor) {
  LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kYes);

  log::info("test1");

  LoggingInfo loggingInfo2(std::move(loggingInfo));

  log::info("test2");
}
}  // namespace cct