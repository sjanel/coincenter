
aux_source_directory(src API_COMMON_SRC)

include(FindOpenSSL)

add_library(coincenter_api-common STATIC ${API_COMMON_SRC})
target_link_libraries(coincenter_api-common PUBLIC coincenter_objects)
target_link_libraries(coincenter_api-common PUBLIC coincenter_api-objects)
target_link_libraries(coincenter_api-common PUBLIC coincenter_http-request)
target_link_libraries(coincenter_api-common PRIVATE OpenSSL::SSL)

target_include_directories(coincenter_api-common PUBLIC include)

function(add_common_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     src/cryptowatchapi.cpp
     LIBRARIES
     coincenter_api-objects
     coincenter_http-request
     coincenter_objects
   )
endfunction()

add_common_test(
  cryptowatchapi_test
  test/cryptowatchapi_test.cpp
)

add_common_test(
  exchangeprivateapi_test
  src/exchangeprivateapi.cpp
  src/exchangepublicapi.cpp
  src/fiatconverter.cpp
  test/exchangeprivateapi_test.cpp
)

add_common_test(
  exchangepublicapi_test
  src/exchangepublicapi.cpp
  src/fiatconverter.cpp
  test/exchangepublicapi_test.cpp
)

add_unit_test( 
    fiatconverter_test
    src/fiatconverter.cpp
    test/fiatconverter_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
  ssl_sha_test
  src/ssl_sha.cpp
  test/ssl_sha_test.cpp
  LIBRARIES
  coincenter_api-common
  OpenSSL::SSL
)


