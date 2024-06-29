aux_source_directory(src ENGINE_SRC)

add_library(coincenter_engine STATIC ${ENGINE_SRC})

target_link_libraries(coincenter_engine PUBLIC coincenter_api-common)
target_link_libraries(coincenter_engine PUBLIC coincenter_api-exchange)
target_link_libraries(coincenter_engine PUBLIC coincenter_api-interface)
target_link_libraries(coincenter_engine PUBLIC coincenter_objects)
target_link_libraries(coincenter_engine PUBLIC coincenter_trading-algorithms)
target_link_libraries(coincenter_engine PUBLIC coincenter_trading-common)
target_include_directories(coincenter_engine PUBLIC include)

add_unit_test(
  coincentercommandfactory_test
  test/coincentercommandfactory_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  coincenteroptions_test
  test/coincenteroptions_test.cpp
  LIBRARIES
  coincenter_engine
)

# As of MSVC 19.38.33135.0, it fails to compile commandlineoptionsparser_test:
# commandlineoptionsparser_test.cpp(333,79): fatal  error C1001: Internal compiler error

if(NOT MSVC)
  add_unit_test(
    commandlineoptionsparser_test
    test/commandlineoptionsparser_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    coincenter_tech
  )
endif()

add_unit_test(
  exchangesorchestrator_private_test
  test/exchangesorchestrator_private_test.cpp
  LIBRARIES
  coincenter_engine
  DIRECTORIES
  ../api/common/test/include
)

add_unit_test(
  exchangesorchestrator_public_test
  test/exchangesorchestrator_public_test.cpp
  LIBRARIES
  coincenter_engine
  DIRECTORIES
  ../api/common/test/include
)

add_unit_test(
  exchangesorchestrator_trade_test
  test/exchangesorchestrator_trade_test.cpp
  LIBRARIES
  coincenter_engine
  DIRECTORIES
  ../api/common/test/include
)

add_unit_test(
  queryresultprinter_public_test
  test/queryresultprinter_public_test.cpp
  LIBRARIES
  coincenter_engine
  DIRECTORIES
  ../api/common/test/include
)

add_unit_test(
  queryresultprinter_private_test
  test/queryresultprinter_private_test.cpp
  LIBRARIES
  coincenter_engine
  DIRECTORIES
  ../api/common/test/include
)

add_unit_test(
  stringoptionparser_test
  test/stringoptionparser_test.cpp
  LIBRARIES
  coincenter_engine
)

add_unit_test(
  transferablecommandresult_test
  test/transferablecommandresult_test.cpp
  LIBRARIES
  coincenter_engine
)
