# Create objects lib
aux_source_directory(src OBJECTS_SRC)

add_coincenter_library(objects STATIC ${OBJECTS_SRC})

target_link_libraries(coincenter_objects PUBLIC coincenter_monitoring)
target_link_libraries(coincenter_objects PUBLIC coincenter_tech)
target_link_libraries(coincenter_objects PUBLIC coincenter_http-request)

add_unit_test(
    balanceportfolio_test
    test/balanceportfolio_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    coincenterinfo_test
    test/coincenterinfo_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    currencycode_test
    test/currencycode_test.cpp
    LIBRARIES
    coincenter_tech
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    exchangeconfig_test
    test/exchangeconfig_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    exchangename_test
    src/exchangename.cpp
    test/exchangename_test.cpp
    LIBRARIES
    coincenter_tech
)

add_unit_test(
    logginginfo_test
    test/logginginfo_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    market_test
    test/market_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    marketorderbook_test
    test/marketorderbook_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    monetaryamount_test
    test/monetaryamount_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    parseloglevel_test
    test/parseloglevel_test.cpp
    LIBRARIES 
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    publictrade_test
    test/publictrade_test.cpp
    LIBRARIES 
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    time-window_test
    test/time-window_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    wallet_test 
    test/wallet_test.cpp 
    LIBRARIES 
    coincenter_objects
    DEFINITIONS 
    CCT_DISABLE_SPDLOG
)
