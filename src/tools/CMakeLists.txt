# Create tools lib
aux_source_directory(src API_TOOLS_SRC)


add_library(coincenter_tools STATIC ${API_TOOLS_SRC})
target_link_libraries(coincenter_tools PUBLIC amc::amc)
target_link_libraries(coincenter_tools PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(coincenter_tools PUBLIC spdlog)
if (MSVC)
  # cURL may have been built with MingW64, allow following extensions to be understood by Visual Studio.
  # https://discourse.cmake.org/t/problem-with-findpackage-curl-required-and-precompiled-curl-7-68-0-for-windows/686/3
  list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a")
  add_compile_definitions(CURL_STATICLIB)
endif()

include(FindCURL)

target_link_libraries(coincenter_tools PRIVATE CURL::libcurl)
target_include_directories(coincenter_tools PUBLIC include)

add_unit_test(
    cct_exception_test
    test/cct_exception_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    commandlineoptionsparser_test
    test/commandlineoptionsparser_test.cpp
)

add_unit_test(
    cct_mathhelpers_test
    test/cct_mathhelpers_test.cpp
)

add_unit_test(
    cachedresult_test
    src/cachedresultvault.cpp
    test/cachedresult_test.cpp
    LIBRARIES
    spdlog
)

add_unit_test(
    curlhandle_test
    src/curlhandle.cpp
    src/curloptions.cpp
    test/curlhandle_test.cpp
    LIBRARIES
    spdlog
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

add_unit_test(
    curloptions_test
    src/curloptions.cpp
    test/curloptions_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    nlohmann_json::nlohmann_json
)

add_unit_test(
    cct_time_helpers_test
    test/cct_time_helpers_test.cpp
)
