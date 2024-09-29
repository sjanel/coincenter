# Create objects lib
aux_source_directory(src BASIC_OBJECTS_SRC)

add_coincenter_library(basic-objects STATIC ${BASIC_OBJECTS_SRC})

target_link_libraries(coincenter_basic-objects PUBLIC coincenter_tech)

add_unit_test(
    currencycode_test
    test/currencycode_test.cpp
    LIBRARIES
    coincenter_tech
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    market_test
    test/market_test.cpp
    LIBRARIES
    coincenter_basic-objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    monetaryamount_test
    test/monetaryamount_test.cpp
    LIBRARIES
    coincenter_basic-objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)