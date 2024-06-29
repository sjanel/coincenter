aux_source_directory(src TRADING-COMMON_SRC)

add_library(coincenter_trading-common STATIC ${TRADING-COMMON_SRC})
target_link_libraries(coincenter_trading-common PUBLIC coincenter_api-objects)
target_link_libraries(coincenter_trading-common PUBLIC coincenter_objects)
target_link_libraries(coincenter_trading-common PUBLIC coincenter_tech)
target_include_directories(coincenter_trading-common PUBLIC include)

add_unit_test(
  algorithm-name-iterator_test
  test/algorithm-name-iterator_test.cpp
  LIBRARIES
  coincenter_trading-common
)