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
     coincenter_objects
     coincenter_tools
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
