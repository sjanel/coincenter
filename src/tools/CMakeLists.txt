# Create tools lib
aux_source_directory(src API_TOOLS_SRC)

add_library(coincenter_tools STATIC ${API_TOOLS_SRC})
target_link_libraries(coincenter_tools PUBLIC coincenter_tech)
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
    curlhandle_test
    src/curlhandle.cpp
    test/curlhandle_test.cpp
    LIBRARIES
    coincenter_tech
    CURL::libcurl
)
