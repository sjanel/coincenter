#include "commonapi_test.hpp"
#include "huobiprivateapi.hpp"
#include "huobipublicapi.hpp"

namespace cct::api {

namespace {
using HuobiAPI = TestAPI<HuobiPublic, HuobiPrivate>;
HuobiAPI testAPI;
}  // namespace

CCT_TEST_ALL(HuobiAPI, testAPI);

}  // namespace cct::api