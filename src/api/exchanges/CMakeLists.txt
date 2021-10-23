function(add_exchange_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   # Recompile wallet.cpp to disable validation of deposit address in file (only for unit tests, no worries)
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     ../../objects/src/wallet.cpp
     DEFINITIONS
     CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE
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
