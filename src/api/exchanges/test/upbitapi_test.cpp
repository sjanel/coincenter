#include "exchangecommonapi_test.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct::api {

namespace {
using UpbitAPI = TestAPI<UpbitPublic, UpbitPrivate>;
UpbitAPI testAPI;
}  // namespace

CCT_TEST_ALL(UpbitAPI, testAPI)

}  // namespace cct::api