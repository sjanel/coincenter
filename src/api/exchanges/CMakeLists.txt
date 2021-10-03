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
     coincenter_objects
     coincenter_tools
   )
endfunction()

add_exchange_test(
  binanceapi_test
  src/binancepublicapi.cpp
  src/binanceprivateapi.cpp
  test/binanceapi_test.cpp
)

add_exchange_test(
  bithumbapi_test
  src/bithumbpublicapi.cpp
  src/bithumbprivateapi.cpp
  test/bithumbapi_test.cpp
)

add_exchange_test(
  huobiapi_test
  src/huobipublicapi.cpp
  src/huobiprivateapi.cpp
  test/huobiapi_test.cpp
)

add_exchange_test(
  kucoinapi_test
  src/kucoinpublicapi.cpp
  src/kucoinprivateapi.cpp
  test/kucoinapi_test.cpp
)

add_exchange_test(
  krakenapi_test
  src/krakenpublicapi.cpp
  src/krakenprivateapi.cpp
  test/krakenapi_test.cpp
)

add_exchange_test(
  upbitapi_test
  src/upbitpublicapi.cpp
  src/upbitprivateapi.cpp
  test/upbitapi_test.cpp
)
