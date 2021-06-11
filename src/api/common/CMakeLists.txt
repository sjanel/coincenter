
aux_source_directory(src API_COMMON_SRC)

include(FindOpenSSL)

add_library(coincenter_apicommon STATIC ${API_COMMON_SRC})
target_link_libraries(coincenter_apicommon PUBLIC coincenter_tools)
target_link_libraries(coincenter_apicommon PUBLIC coincenter_objects) 
target_link_libraries(coincenter_apicommon PRIVATE OpenSSL::SSL)

target_include_directories(coincenter_apicommon PUBLIC include)

add_unit_test(
  cryptowatchapi_test
  src/cryptowatchapi.cpp
  test/cryptowatchapi_test.cpp
  LIBRARIES
  coincenter_objects
  coincenter_tools
)

add_unit_test(
  exchangeprivateapi_test
  src/cryptowatchapi.cpp
  src/exchangeprivateapi.cpp
  src/exchangepublicapi.cpp
  src/tradeinfo.cpp
  src/tradeoptions.cpp
  test/exchangeprivateapi_test.cpp
  LIBRARIES
  coincenter_objects
  coincenter_tools
)

add_unit_test(
  exchangepublicapi_test
  src/cryptowatchapi.cpp
  src/exchangepublicapi.cpp
  test/exchangepublicapi_test.cpp
  LIBRARIES
  coincenter_objects
  coincenter_tools
)