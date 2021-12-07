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
    cct_exception_test
    test/cct_exception_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    cct_hash_test
    test/cct_hash_test.cpp
)

add_unit_test(
    commandlineoptionsparser_test
    src/commandlineoption.cpp
    test/commandlineoptionsparser_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    mathhelpers_test
    test/mathhelpers_test.cpp
)

add_unit_test(
    timestring_test
    src/timestring.cpp
    test/timestring_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    cachedresult_test
    src/cachedresultvault.cpp
    test/cachedresult_test.cpp
    LIBRARIES
    spdlog
)

add_unit_test(
    flatkeyvaluestring_test
    test/flatkeyvaluestring_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
    LIBRARIES
    nlohmann_json::nlohmann_json
)

add_unit_test(
    simpletable_test
    src/simpletable.cpp
    test/simpletable_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    stringhelpers_test
    test/stringhelpers_test.cpp
    DEFINITIONS
    CCT_DISABLE_SPDLOG
)

add_unit_test(
    timehelpers_test
    test/timehelpers_test.cpp
)