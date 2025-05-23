if(CCT_ENABLE_PROTO)
  aux_source_directory(src SERIALIZATION_SRC)

  list(APPEND SERIALIZATION_SRC "${CMAKE_CURRENT_LIST_DIR}/proto/market-order-book.proto")
  list(APPEND SERIALIZATION_SRC "${CMAKE_CURRENT_LIST_DIR}/proto/public-trade.proto")
else()
  set(SERIALIZATION_SRC "src/dummy-market-data-serializer.cpp" "src/dummy-market-data-deserializer.cpp")
endif()

add_coincenter_library(serialization STATIC ${SERIALIZATION_SRC})

target_link_libraries(coincenter_serialization PUBLIC coincenter_objects)

if(CCT_ENABLE_PROTO)
  set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

  target_include_directories(coincenter_serialization PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

  target_link_libraries(coincenter_serialization PUBLIC protobuf::libprotobuf ZLIB::ZLIB)

  protobuf_generate(
    TARGET coincenter_serialization
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto"
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}"
    OUT_VAR PROTO_GENERATED_FILES
  )

  if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.27.0")
    set_source_files_properties(${PROTO_GENERATED_FILES}
      PROPERTIES
      SKIP_LINTING ON
    )
  endif()

  add_unit_test(
    continuous-iterator_test
    test/continuous-iterator_test.cpp
  )

  add_unit_test(
    proto-market-order-book-converter_test
    test/proto-market-order-book-converter_test.cpp
    LIBRARIES
    coincenter_serialization
  )

  add_unit_test(
    proto-multiple-messages-handler_test
    test/proto-multiple-messages-handler_test.cpp
    LIBRARIES
    coincenter_serialization
  )

  add_unit_test(
    proto-public-trade-converter_test
    test/proto-public-trade-converter_test.cpp
    LIBRARIES
    coincenter_serialization
  )

  add_unit_test(
    proto-serialization-and-deserialization_test
    test/proto-serialization-and-deserialization_test.cpp
    LIBRARIES
    coincenter_serialization
  )

  add_unit_test(
    serialization-tools_test
    test/serialization-tools_test.cpp
    LIBRARIES
    coincenter_serialization
  )

endif()
