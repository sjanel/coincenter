aux_source_directory(src TRADING-COMMON_SRC)

add_coincenter_library(trading-common STATIC ${TRADING-COMMON_SRC})

target_link_libraries(coincenter_trading-common PUBLIC coincenter_api-objects)
target_link_libraries(coincenter_trading-common PUBLIC coincenter_objects)
target_link_libraries(coincenter_trading-common PUBLIC coincenter_tech)

add_unit_test(
  algorithm-name-iterator_test
  test/algorithm-name-iterator_test.cpp
  LIBRARIES
  coincenter_trading-common
)