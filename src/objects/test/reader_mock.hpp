#pragma once

#include <gmock/gmock.h>

#include "reader.hpp"

namespace cct {

class MockReader : public Reader {
 public:
  MOCK_METHOD(string, readAll, (), (const override));
};

}  // namespace cct