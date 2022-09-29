
include(FetchContent)

FetchContent_Declare(
  jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp
  GIT_TAG        master
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(jwt-cpp)

aux_source_directory(src API_EXCHANGES_SRC)

add_library(coincenter_exchangeapi STATIC ${API_EXCHANGES_SRC})
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_objects)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_apicommon)
target_link_libraries(coincenter_exchangeapi PRIVATE coincenter_monitoring)
target_link_libraries(coincenter_exchangeapi PRIVATE jwt-cpp)

target_include_directories(coincenter_exchangeapi PUBLIC include)

function(add_exchange_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     LIBRARIES
     jwt-cpp
     coincenter_apicommon
     coincenter_exchangeapi
   )
endfunction()

add_exchange_test(
  binanceapi_test
  test/binanceapi_test.cpp
)

add_exchange_test(
  bithumbapi_test
  test/bithumbapi_test.cpp
)

add_exchange_test(
  huobiapi_test
  test/huobiapi_test.cpp
)

add_exchange_test(
  kucoinapi_test
  test/kucoinapi_test.cpp
)

add_exchange_test(
  krakenapi_test
  test/krakenapi_test.cpp
)

add_exchange_test(
  upbitapi_test
  test/upbitapi_test.cpp
)
