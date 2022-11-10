# Create tools lib
aux_source_directory(src API_OBJECTS_SRC)

add_library(coincenter_api-objects STATIC ${API_OBJECTS_SRC})
target_link_libraries(coincenter_api-objects PRIVATE coincenter_tech)
target_link_libraries(coincenter_api-objects PRIVATE coincenter_objects)

target_include_directories(coincenter_api-objects PUBLIC include)
 
add_unit_test(
  depositsconstraints_test
  src/depositsconstraints.cpp
  test/depositsconstraints_test.cpp
  LIBRARIES
  coincenter_objects
  DEFINITIONS
  CCT_DISABLE_SPDLOG
)

add_unit_test(
  ordersconstraints_test
  src/ordersconstraints.cpp
  test/ordersconstraints_test.cpp
  LIBRARIES
  coincenter_objects
  DEFINITIONS
  CCT_DISABLE_SPDLOG
)

add_unit_test(
  recentdeposit_test
  src/recentdeposit.cpp
  test/recentdeposit_test.cpp
  LIBRARIES
  coincenter_objects
  coincenter_tools
)