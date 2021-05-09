include(FetchContent)

FetchContent_Declare(
  jwt-cpp
  QUIET
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp
  GIT_TAG        origin/master
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(jwt-cpp)

#-------------------------------------------------------------------------------
# Add library exchangeapi
#-------------------------------------------------------------------------------
aux_source_directory(common/src API_COMMON_SRC)
aux_source_directory(exchanges/src API_EXCHANGES_SRC)
aux_source_directory(interface/src API_INTERFACE_SRC)

include(FindOpenSSL)

add_library(coincenter_exchangeapi STATIC ${API_COMMON_SRC} ${API_EXCHANGES_SRC} ${API_INTERFACE_SRC})
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_tools)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_objects)
target_link_libraries(coincenter_exchangeapi PRIVATE OpenSSL::SSL)
target_link_libraries(coincenter_exchangeapi PRIVATE jwt-cpp)

target_include_directories(coincenter_exchangeapi PUBLIC exchanges/include)
target_include_directories(coincenter_exchangeapi PUBLIC common/include)
target_include_directories(coincenter_exchangeapi PUBLIC interface/include)

add_unit_test(
  binanceapi_test
  common/src/exchangepublicapi.cpp
  exchanges/src/binancepublicapi.cpp
  exchanges/src/binanceprivateapi.cpp
  exchanges/test/binanceapi_test.cpp
  LIBRARIES
  coincenter_exchangeapi
)

add_unit_test(
  bithumbapi_test
  common/src/exchangepublicapi.cpp
  exchanges/src/bithumbpublicapi.cpp
  exchanges/src/bithumbprivateapi.cpp
  exchanges/test/bithumbapi_test.cpp
  LIBRARIES
  coincenter_exchangeapi
)

add_unit_test(
  cryptowatchapi_test
  exchanges/src/cryptowatchapi.cpp
  exchanges/test/cryptowatchapi_test.cpp
  LIBRARIES
  coincenter_exchangeapi
)

add_unit_test(
  krakenapi_test
  common/src/exchangepublicapi.cpp
  exchanges/src/cryptowatchapi.cpp
  exchanges/src/krakenpublicapi.cpp
  exchanges/src/krakenprivateapi.cpp
  exchanges/test/krakenapi_test.cpp
  LIBRARIES
  coincenter_exchangeapi
)

add_unit_test(
  upbitapi_test
  common/src/exchangepublicapi.cpp
  exchanges/src/upbitpublicapi.cpp
  exchanges/test/upbitapi_test.cpp
  LIBRARIES
  coincenter_exchangeapi
)