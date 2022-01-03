#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "commonapi_test.hpp"

namespace cct::api {

namespace {
using BithumbAPI = TestAPI<BithumbPublic, BithumbPrivate>;
BithumbAPI testAPI;
}  // namespace

CCT_TEST_ALL(BithumbAPI, testAPI);

}  // namespace cct::api