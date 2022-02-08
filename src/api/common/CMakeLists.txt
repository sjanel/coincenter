
aux_source_directory(src API_COMMON_SRC)

include(FindOpenSSL)

add_library(coincenter_apicommon STATIC ${API_COMMON_SRC})
target_link_libraries(coincenter_apicommon PUBLIC coincenter_objects)
target_link_libraries(coincenter_apicommon PRIVATE OpenSSL::SSL)

target_include_directories(coincenter_apicommon PUBLIC include)

function(add_common_test name)
   set(oneValueArgs)
   set(multiValueArgs)
   cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")
   
   add_unit_test(
     ${name} 
     ${MY_UNPARSED_ARGUMENTS}
     src/cryptowatchapi.cpp
     LIBRARIES
     coincenter_objects
     coincenter_tools
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
  src/tradedamounts.cpp
  src/tradeoptions.cpp
  test/exchangeprivateapi_test.cpp
)

add_common_test(
  exchangepublicapi_test
  src/exchangepublicapi.cpp
  test/exchangepublicapi_test.cpp
)

add_unit_test(
  recentdeposit_test
  src/recentdeposit.cpp
  test/recentdeposit_test.cpp
  LIBRARIES
  coincenter_objects
  coincenter_tools
)