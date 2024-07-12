aux_source_directory(src TRADING-ALGORITHMS_SRC)

add_coincenter_library(trading-algorithms STATIC ${TRADING-ALGORITHMS_SRC})

target_link_libraries(coincenter_trading-algorithms PUBLIC coincenter_api-objects)
target_link_libraries(coincenter_trading-algorithms PUBLIC coincenter_objects)
target_link_libraries(coincenter_trading-algorithms PUBLIC coincenter_tech)

target_link_libraries(coincenter_trading-algorithms PUBLIC coincenter_trading-common)
target_link_libraries(coincenter_trading-algorithms PUBLIC coincenter_trading-indicators)

add_unit_test(
  market-trader-suite_test
  test/market-trader-suite_test.cpp
  LIBRARIES
  coincenter_trading-algorithms
)
