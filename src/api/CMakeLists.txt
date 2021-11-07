include(FetchContent)

FetchContent_Declare(
  jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp
  GIT_TAG        v0.5.1
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(jwt-cpp)

#-------------------------------------------------------------------------------
# Add library exchangeapi
#-------------------------------------------------------------------------------
aux_source_directory(exchanges/src API_EXCHANGES_SRC)
aux_source_directory(interface/src API_INTERFACE_SRC)

add_library(coincenter_exchangeapi STATIC ${API_EXCHANGES_SRC} ${API_INTERFACE_SRC})
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_tools)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_objects)
target_link_libraries(coincenter_exchangeapi PUBLIC coincenter_apicommon)
target_link_libraries(coincenter_exchangeapi PRIVATE coincenter_monitoring)
target_link_libraries(coincenter_exchangeapi PRIVATE jwt-cpp)

target_include_directories(coincenter_exchangeapi PUBLIC exchanges/include)
target_include_directories(coincenter_exchangeapi PUBLIC interface/include)

add_subdirectory(common)
add_subdirectory(exchanges)
add_subdirectory(interface)