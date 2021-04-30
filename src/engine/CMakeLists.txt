aux_source_directory(src ENGINE_SRC)

if (CCT_BUILD_EXEC_INSTEAD_OF_LIB)
  add_executable (coincenter ${ENGINE_SRC})
else(CCT_BUILD_EXEC_INSTEAD_OF_LIB)
  list(REMOVE_ITEM ENGINE_SRC "src/main.cpp")
  add_library(coincenter STATIC ${ENGINE_SRC})
endif(CCT_BUILD_EXEC_INSTEAD_OF_LIB)

target_link_libraries(coincenter PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(coincenter PRIVATE spdlog)
target_link_libraries(coincenter PUBLIC coincenter_exchangeapi)
target_link_libraries(coincenter PUBLIC coincenter_objects)
target_link_libraries(coincenter PUBLIC coincenter_tools)
target_include_directories(coincenter PUBLIC include)

set_target_properties (coincenter PROPERTIES 
          COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT"
          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set_target_properties(coincenter PROPERTIES VERSION ${PROJECT_VERSION})

# Enable LTO with coincenter in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET coincenter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  message(STATUS "Activate LTO for coincenter")
endif(CMAKE_BUILD_TYPE STREQUAL "Release")