
aux_source_directory(src API_COMMON_SRC)

add_library(coincenter_api-common STATIC ${API_COMMON_SRC})

target_link_libraries(coincenter_api-common PUBLIC coincenter_api-objects)
target_link_libraries(coincenter_api-common PUBLIC coincenter_objects)
target_link_libraries(coincenter_api-common PUBLIC coincenter_http-request)
target_link_libraries(coincenter_api-common PUBLIC coincenter_serialization)
target_link_libraries(coincenter_api-common PRIVATE OpenSSL::SSL)

target_include_directories(coincenter_api-common PUBLIC include)

function(add_common_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     LIBRARIES
     coincenter_api-common
     DIRECTORIES
     test/include
   )
endfunction()

add_common_test(
  commonapi_test
  test/commonapi_test.cpp
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

add_common_test(
  withdrawalfees-crawler_test
  test/withdrawalfees-crawler_test.cpp
)
