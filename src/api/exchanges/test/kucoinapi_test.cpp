#include "commonapi_test.hpp"
#include "kucoinprivateapi.hpp"
#include "kucoinpublicapi.hpp"

namespace cct::api {

namespace {
using KucoinAPI = TestAPI<KucoinPublic, KucoinPrivate>;
KucoinAPI testAPI;
}  // namespace

CCT_TEST_ALL(KucoinAPI, testAPI);

}  // namespace cct::api