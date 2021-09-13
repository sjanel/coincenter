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
aux_source_directory(exchanges/src API_EXCHANGES_SRC)
aux_source_directory(interface/src API_INTERFACE_SRC)

add_library(coincenter_exchangeapi STATIC ${API_EXCHANGES_SRC} ${API_INTERFACE_SRC})
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_tools)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_objects)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_apicommon)
target_link_libraries(coincenter_exchangeapi PRIVATE jwt-cpp)

target_include_directories(coincenter_exchangeapi PUBLIC exchanges/include)
target_include_directories(coincenter_exchangeapi PUBLIC interface/include)

function(add_exchange_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   if(CCT_ENABLE_TESTS)
     # Recompile wallet.cpp to disable validation of deposit address in file (only for unit tests, no worries)
     add_unit_test(
       ${name} 
       ${MY_UNPARSED_ARGUMENTS}
       ../objects/src/wallet.cpp
       DEFINITIONS
       CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE
       LIBRARIES
       jwt-cpp
       coincenter_apicommon
       coincenter_objects
       coincenter_tools
     )
     target_include_directories(${name} PRIVATE exchanges/include)
   endif() 
endfunction()

add_exchange_test(
  binanceapi_test
  exchanges/src/binancepublicapi.cpp
  exchanges/src/binanceprivateapi.cpp
  exchanges/test/binanceapi_test.cpp
)

add_exchange_test(
  bithumbapi_test
  exchanges/src/bithumbpublicapi.cpp
  exchanges/src/bithumbprivateapi.cpp
  exchanges/test/bithumbapi_test.cpp
)

add_exchange_test(
  huobiapi_test
  exchanges/src/huobipublicapi.cpp
  exchanges/src/huobiprivateapi.cpp
  exchanges/test/huobiapi_test.cpp
)

add_exchange_test(
  kucoinapi_test
  exchanges/src/kucoinpublicapi.cpp
  exchanges/src/kucoinprivateapi.cpp
  exchanges/test/kucoinapi_test.cpp
)

add_exchange_test(
  krakenapi_test
  exchanges/src/krakenpublicapi.cpp
  exchanges/src/krakenprivateapi.cpp
  exchanges/test/krakenapi_test.cpp
)

add_exchange_test(
  upbitapi_test
  exchanges/src/upbitpublicapi.cpp
  exchanges/src/upbitprivateapi.cpp
  exchanges/test/upbitapi_test.cpp
)

add_subdirectory(common)