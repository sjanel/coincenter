# Lowest level services for coincenter
aux_source_directory(src API_TECH_SRC)

add_library(coincenter_tech STATIC ${API_TECH_SRC})
target_link_libraries(coincenter_tech PUBLIC nlohmann_json::nlohmann_json)
if (LINK_AMC)
  target_link_libraries(coincenter_tech PUBLIC amc::amc)
endif()
target_link_libraries(coincenter_tech PUBLIC spdlog)

target_include_directories(coincenter_tech PUBLIC include)

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
    codec_test
    src/codec.cpp
    test/codec_test.cpp
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
    mathhelpers_test
    test/mathhelpers_test.cpp
)

add_unit_test(
    parseloglevel_test
    src/parseloglevel.cpp
    test/parseloglevel_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
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
    stringhelpers_test
    test/stringhelpers_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
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
