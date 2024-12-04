# Lowest level services for coincenter
aux_source_directory(src API_TECH_SRC)

add_coincenter_library(tech STATIC ${API_TECH_SRC})

target_link_libraries(coincenter_tech PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(coincenter_tech PUBLIC glaze::glaze)

if (LINK_AMC)
  target_link_libraries(coincenter_tech PUBLIC amc::amc)
endif()
target_link_libraries(coincenter_tech PUBLIC spdlog)

add_unit_test(
    cachedresult_test
    test/cachedresult_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    cct_exception_test
    test/cct_exception_test.cpp
    LIBRARIES
    spdlog
)

add_unit_test(
    cct_hash_test
    test/cct_hash_test.cpp
)

add_unit_test(
    char-hexadecimal-converter_test
    test/char-hexadecimal-converter_test.cpp
)

add_unit_test(
    base64_test
    src/base64.cpp
    test/base64_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    durationstring_test
    src/durationstring.cpp
    test/durationstring_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    levenshteindistancecalculator_test
    src/levenshteindistancecalculator.cpp
    test/levenshteindistancecalculator_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    flatkeyvaluestring_test
    test/flatkeyvaluestring_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    nlohmann_json::nlohmann_json
)

if(MSVC)
    add_unit_test(
        gethostname_test
        src/gethostname.cpp
        test/gethostname_test.cpp
        DEFINITIONS
        CCT_DISABLE_SPDLOG
        LIBRARIES
        wsock32
        ws2_32
    )
else()
    add_unit_test(
        gethostname_test
        src/gethostname.cpp
        test/gethostname_test.cpp
        DEFINITIONS
        CCT_DISABLE_SPDLOG
    )
endif()

add_unit_test(
    ndigits_test
    test/ndigits_test.cpp
)

add_unit_test(
    ipow_test
    test/ipow_test.cpp
)

add_unit_test(
    overflow-check_test
    test/overflow-check_test.cpp
)

add_unit_test(
    simpletable_test
    src/simpletable.cpp
    test/simpletable_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    static_string_view_helpers_test
    test/static_string_view_helpers_test.cpp
)

add_unit_test(
    stringconv_test
    test/stringconv_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    strnlen_test
    test/strnlen_test.cpp
)

add_unit_test(
    threadpool_test
    test/threadpool_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    timedef_test
    test/timedef_test.cpp
)

add_unit_test(
    timestring_test
    src/timestring.cpp
    test/timestring_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    unitsparser_test
    src/unitsparser.cpp
    test/unitsparser_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    url-encode_test.cpp
    test/url-encode_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    utf8_test
    test/utf8_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)