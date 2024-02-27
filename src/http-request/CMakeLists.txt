aux_source_directory(src API_IO_TOOLS_SRC)

add_library(coincenter_http-request STATIC ${API_IO_TOOLS_SRC})
target_link_libraries(coincenter_http-request PUBLIC coincenter_tech)
target_link_libraries(coincenter_http-request PRIVATE coincenter_monitoring)
target_link_libraries(coincenter_http-request PRIVATE CURL::libcurl)
target_include_directories(coincenter_http-request PUBLIC include)

add_unit_test(
    besturlpicker_test
    src/besturlpicker.cpp
    test/besturlpicker_test.cpp
    LIBRARIES
    coincenter_tech
)

add_unit_test(
    curlhandle_test
    test/curlhandle_test.cpp
    LIBRARIES
    coincenter_http-request
)

add_unit_test(
    permanentcurloptions_test
    test/permanentcurloptions_test.cpp
    LIBRARIES
    coincenter_tech
)
