aux_source_directory(src SCHEMA_SRC)

add_coincenter_library(schema STATIC ${SCHEMA_SRC})

target_link_libraries(coincenter_schema PUBLIC coincenter_basic-objects)

add_unit_test(
    deposit-addresses_test
    test/deposit-addresses_test.cpp
    LIBRARIES
    coincenter_schema
)