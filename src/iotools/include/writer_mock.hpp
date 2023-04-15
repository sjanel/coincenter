#pragma once

#include <gmock/gmock.h>

#include "writer.hpp"

namespace cct {

class MockWriter : public Writer {
 public:
  MOCK_METHOD(int, write, (const json &), (const override));
};

}  // namespace cct