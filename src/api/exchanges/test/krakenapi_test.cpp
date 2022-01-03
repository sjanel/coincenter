#include "commonapi_test.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"

namespace cct::api {

namespace {
using KrakenAPI = TestAPI<KrakenPublic, KrakenPrivate>;
KrakenAPI testAPI;
}  // namespace

CCT_TEST_ALL(KrakenAPI, testAPI);

}  // namespace cct::api