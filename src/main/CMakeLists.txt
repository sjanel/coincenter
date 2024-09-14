aux_source_directory(src MAIN_SRC)

include(CheckIPOSupported)

if(CCT_BUILD_EXEC)
  add_coincenter_executable(coincenter ${MAIN_SRC})

  check_ipo_supported(RESULT result OUTPUT output LANGUAGES CXX)

  if(result)
    # Enable LTO with coincenter in Release mode
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
      set_property(TARGET coincenter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
      message(STATUS "Activate LTO for coincenter")
    endif()
  else()
    message(WARNING "IPO is not supported: ${output}")
  endif()

else()
  list(REMOVE_ITEM ENGINE_SRC "src/main.cpp")
  add_coincenter_library(coincenter STATIC ${MAIN_SRC})
endif()

target_link_libraries(coincenter PUBLIC coincenter_engine)

set_target_properties(coincenter PROPERTIES
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_unit_test(
  processcommandsfromcli_test
  src/processcommandsfromcli.cpp
  test/processcommandsfromcli_test.cpp
  LIBRARIES
  coincenter_engine
)
