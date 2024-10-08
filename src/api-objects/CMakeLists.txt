aux_source_directory(src API_OBJECTS_SRC)

add_coincenter_library(api-objects STATIC ${API_OBJECTS_SRC})

target_link_libraries(coincenter_api-objects PUBLIC coincenter_tech)
target_link_libraries(coincenter_api-objects PUBLIC coincenter_objects)

add_unit_test(
    baseconstraints_test
    test/baseconstraints_test.cpp
)

add_unit_test(
    closed-order_test
    test/closed-order_test.cpp
    LIBRARIES
    coincenter_api-objects
)

add_unit_test(
    deposit_test
    test/deposit_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    withdrawsordepositsconstraints_test
    test/withdrawsordepositsconstraints_test.cpp
    LIBRARIES
    coincenter_api-objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    ordersconstraints_test
    src/ordersconstraints.cpp
    test/ordersconstraints_test.cpp
    LIBRARIES
    coincenter_objects
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    recentdeposit_test
    src/recentdeposit.cpp
    test/recentdeposit_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    tradeoptions_test
    src/tradeoptions.cpp
    test/tradeoptions_test.cpp
    LIBRARIES
    coincenter_objects
)

add_unit_test(
    withdraw_test
    test/withdraw_test.cpp
    LIBRARIES
    coincenter_objects
)