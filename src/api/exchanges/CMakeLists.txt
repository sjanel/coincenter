
include(FetchContent)

aux_source_directory(src API_EXCHANGES_SRC)

add_coincenter_library(api-exchange STATIC ${API_EXCHANGES_SRC})

target_link_libraries(coincenter_api-exchange PUBLIC coincenter_objects)
target_link_libraries(coincenter_api-exchange PUBLIC coincenter_api-common)
target_link_libraries(coincenter_api-exchange PRIVATE coincenter_monitoring)
target_link_libraries(coincenter_api-exchange PRIVATE jwt-cpp)

function(add_exchange_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     LIBRARIES
     jwt-cpp
     coincenter_api-exchange
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
  bithumb_place_order_test
  test/bithumb_place_order_test.cpp
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
