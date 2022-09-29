
aux_source_directory(src API_INTERFACE_SRC)

add_library(coincenter_interfaceapi STATIC ${API_INTERFACE_SRC})
target_link_libraries(coincenter_interfaceapi PUBLIC coincenter_exchangeapi)
target_link_libraries(coincenter_interfaceapi PRIVATE coincenter_monitoring)

target_include_directories(coincenter_interfaceapi PUBLIC include)

add_unit_test(
    exchangeretrieverbase_test
    test/exchangeretrieverbase_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    coincenter_objects
)