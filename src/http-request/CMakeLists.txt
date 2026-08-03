aux_source_directory(src API_IO_TOOLS_SRC)

add_coincenter_library(http-request STATIC ${API_IO_TOOLS_SRC})

target_link_libraries(coincenter_http-request PUBLIC coincenter_tech)
target_link_libraries(coincenter_http-request PRIVATE coincenter_monitoring)
target_link_libraries(coincenter_http-request PRIVATE aeronet::aeronet)

add_unit_test(
    besturlpicker_test
    src/besturlpicker.cpp
    test/besturlpicker_test.cpp
    LIBRARIES
    coincenter_tech
)

add_unit_test(
    httpclient_test
    test/httpclient_test.cpp
    LIBRARIES
    coincenter_http-request
    aeronet::aeronet
)

add_unit_test(
    permanentrequestoptions_test
    test/permanentrequestoptions_test.cpp
    LIBRARIES
    coincenter_tech
)
