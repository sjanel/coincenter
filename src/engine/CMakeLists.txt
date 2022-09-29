aux_source_directory(src ENGINE_SRC)

add_library(coincenter_engine STATIC ${ENGINE_SRC})

target_link_libraries(coincenter_engine PUBLIC coincenter_apicommon)
target_link_libraries(coincenter_engine PUBLIC coincenter_exchangeapi)
target_link_libraries(coincenter_engine PUBLIC coincenter_interfaceapi)
target_link_libraries(coincenter_engine PUBLIC coincenter_objects)
target_include_directories(coincenter_engine PUBLIC include)

add_unit_test(
  commandlineoptionsparser_test
  test/commandlineoptionsparser_test.cpp
  DEFINITIONS
  CCT_DISABLE_SPDLOG
  LIBRARIES
  coincenter_tech
)

add_unit_test(
  exchangesorchestrator_private_test
  test/exchangesorchestrator_private_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  exchangesorchestrator_public_test
  test/exchangesorchestrator_public_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  exchangesorchestrator_trade_test
  test/exchangesorchestrator_trade_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  queryresultprinter_test
  test/queryresultprinter_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  stringoptionparser_test
  test/stringoptionparser_test.cpp
  LIBRARIES
  coincenter_engine
)