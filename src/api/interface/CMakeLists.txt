aux_source_directory(src API_INTERFACE_SRC)

add_library(coincenter_api-interface STATIC ${API_INTERFACE_SRC})
target_link_libraries(coincenter_api-interface PUBLIC coincenter_api-exchange)
target_link_libraries(coincenter_api-interface PRIVATE coincenter_monitoring)

target_include_directories(coincenter_api-interface PUBLIC include)

add_unit_test(
    exchangeretriever_test
    test/exchangeretriever_test.cpp
    LIBRARIES
    coincenter_api-interface
    DIRECTORIES
    ../common/test/include
)