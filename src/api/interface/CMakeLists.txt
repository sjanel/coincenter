add_unit_test(
    exchangeretrieverbase_test
    test/exchangeretrieverbase_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    coincenter_objects
)