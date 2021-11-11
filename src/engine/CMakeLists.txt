aux_source_directory(src ENGINE_SRC)

if (CCT_BUILD_EXEC)
  add_executable (coincenter ${ENGINE_SRC})

  # Enable LTO with coincenter in Release mode
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET coincenter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "Activate LTO for coincenter")
  endif()

else()

  list(REMOVE_ITEM ENGINE_SRC "src/main.cpp")
  add_library(coincenter STATIC ${ENGINE_SRC})

endif()

target_link_libraries(coincenter PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(coincenter PRIVATE spdlog)
target_link_libraries(coincenter PUBLIC coincenter_exchangeapi)
target_link_libraries(coincenter PUBLIC coincenter_objects)
target_include_directories(coincenter PUBLIC include)

set_target_properties(coincenter PROPERTIES 
                                    VERSION ${PROJECT_VERSION}
                                    COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT"
                                    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)    

add_unit_test(
    stringoptionparser_test
    src/stringoptionparser.cpp
    test/stringoptionparser_test.cpp
    LIBRARIES
    coincenter_objects
    coincenter_tools
)
