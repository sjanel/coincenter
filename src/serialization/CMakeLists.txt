if(CCT_ENABLE_PROTO)
  aux_source_directory(src SERIALIZATION_SRC)

  list(APPEND SERIALIZATION_SRC "${CMAKE_CURRENT_LIST_DIR}/proto/market-order-book.proto")
  list(APPEND SERIALIZATION_SRC "${CMAKE_CURRENT_LIST_DIR}/proto/public-trade.proto")
else()
  set(SERIALIZATION_SRC "src/dummy-market-data-serializer.cpp" "src/dummy-market-data-deserializer.cpp")
endif()

add_library(coincenter_serialization STATIC ${SERIALIZATION_SRC})

target_include_directories(coincenter_serialization PUBLIC include)
target_link_libraries(coincenter_serialization PUBLIC coincenter_objects)

if(CCT_ENABLE_PROTO)
  set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

  target_include_directories(coincenter_serialization PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

  target_link_libraries(coincenter_serialization PUBLIC ${Protobuf_LIBRARIES})

  if(MSVC)
    # Not sure why it's needed for Windows.
    target_link_libraries(libprotobuf PUBLIC ZLIB::ZLIB)
  endif()
  target_link_libraries(coincenter_serialization PUBLIC ZLIB::ZLIB)

  protobuf_generate(
    TARGET coincenter_serialization
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto"
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}"
  )

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
