# Create objects lib
aux_source_directory(src OBJECTS_SRC)

add_library(coincenter_objects STATIC ${OBJECTS_SRC})
target_link_libraries(coincenter_objects PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(coincenter_objects PRIVATE spdlog)
target_link_libraries(coincenter_objects PUBLIC coincenter_tools)
target_include_directories(coincenter_objects PUBLIC include)

add_unit_test(
    balanceportfolio_test
    test/balanceportfolio_test.cpp
    LIBRARIES
    coincenter_objects
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
    test/exchangeinfo_test.cpp
    LIBRARIES
    nlohmann_json::nlohmann_json
    coincenter_objects
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
    test/fiatconverter_test.cpp
    LIBRARIES
    nlohmann_json::nlohmann_json
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