#include "logginginfo.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(LoggingInfo, DefaultConstructor) {
  LoggingInfo loggingInfo;

  log::info("test");
}

TEST(LoggingInfo, ConstructorFromJson) {
  json generalConfigJsonLogPart;

  generalConfigJsonLogPart.emplace("maxFileSize", "1Mi");
  generalConfigJsonLogPart.emplace("maxNbFiles", 42);
  generalConfigJsonLogPart.emplace("console", "debug");
  generalConfigJsonLogPart.emplace("file", "trace");

  LoggingInfo loggingInfo(generalConfigJsonLogPart);

  log::info("test");
}

TEST(LoggingInfo, ReentrantTest) {
  {
    LoggingInfo loggingInfo;

    log::info("test1");
  }

  {
    LoggingInfo loggingInfo;

    log::info("test2");
  }
}

TEST(LoggingInfo, MoveConstructor) {
  LoggingInfo loggingInfo;

  log::info("test1");

  LoggingInfo loggingInfo2(std::move(loggingInfo));

  log::info("test2");
}
}  // namespace cct