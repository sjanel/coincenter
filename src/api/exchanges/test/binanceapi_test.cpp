#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "commonapi_test.hpp"

namespace cct::api {

namespace {
using BinanceAPI = TestAPI<BinancePublic, BinancePrivate>;
BinanceAPI testAPI;
}  // namespace

CCT_TEST_ALL(BinanceAPI, testAPI)

}  // namespace cct::api