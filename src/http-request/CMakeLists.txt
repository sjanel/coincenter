aux_source_directory(src API_IO_TOOLS_SRC)

add_library(coincenter_http-request STATIC ${API_IO_TOOLS_SRC})
target_link_libraries(coincenter_http-request PUBLIC coincenter_tech)
target_link_libraries(coincenter_http-request PRIVATE coincenter_monitoring)
if (MSVC)
  # cURL may have been built with MingW64, allow following extensions to be understood by Visual Studio.
  # https://discourse.cmake.org/t/problem-with-findpackage-curl-required-and-precompiled-curl-7-68-0-for-windows/686/3
  list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a")
  add_compile_definitions(CURL_STATICLIB)
endif()

include(FindCURL)

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
