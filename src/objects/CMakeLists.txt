# Create objects lib
aux_source_directory(src OBJECTS_SRC)

add_library(coincenter_objects STATIC ${OBJECTS_SRC})
target_link_libraries(coincenter_objects PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(coincenter_objects PRIVATE spdlog)
target_link_libraries(coincenter_objects PUBLIC coincenter_tools)
target_include_directories(coincenter_objects PUBLIC include)

add_unit_test(
    balanceportfolio_test
    src/balanceportfolio.cpp
    src/currencycode.cpp
    src/monetaryamount.cpp
    test/balanceportfolio_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    currencycode_test
    src/currencycode.cpp
    test/currencycode_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    exchangeinfo_test
    src/currencycode.cpp
    src/exchangeinfo.cpp
    src/monetaryamount.cpp
    test/exchangeinfo_test.cpp
    LIBRARIES
    nlohmann_json::nlohmann_json
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    exchangename_test
    src/exchangename.cpp
    test/exchangename_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test( 
    fiatconverter_test
    src/currencycode.cpp
    src/fiatconverter.cpp
    src/monetaryamount.cpp
    test/fiatconverter_test.cpp
    LIBRARIES
    nlohmann_json::nlohmann_json
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    marketorderbook_test
    src/currencycode.cpp
    src/market.cpp
    src/marketorderbook.cpp
    src/monetaryamount.cpp
    test/marketorderbook_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    monetaryamount_test
    src/currencycode.cpp
    src/monetaryamount.cpp
    test/monetaryamount_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)