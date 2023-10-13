cmake_minimum_required(VERSION 3.15)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.21.0.0
  DESCRIPTION "A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface"
  LANGUAGES CXX)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

#
# MAIN_PROJECT CHECK
# determine if coincenter is built as a subproject or if it is the main project
#
set(MAIN_PROJECT OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

set(CCT_ASAN_BUILD OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # In Debug, activate asan by default
  set(CCT_ASAN_BUILD ON)
  find_program(CLANG_TIDY "clang-tidy")
endif()

option(CCT_ENABLE_TESTS "Build the unit tests" ${MAIN_PROJECT})
option(CCT_BUILD_EXEC "Build an executable instead of a static library" ${MAIN_PROJECT})
option(CCT_ENABLE_ASAN "Compile with AddressSanitizer" ${CCT_ASAN_BUILD})
option(CCT_ENABLE_CLANG_TIDY "Compile with clang-tidy checks" OFF)
option(CCT_BUILD_PROMETHEUS_FROM_SRC "Fetch and build from prometheus-cpp sources" OFF)

set(CCT_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data" CACHE PATH "Needed data directory for coincenter. Can also be overriden at runtime with this environment variable")

if(EXISTS ${CCT_DATA_DIR})
  message(STATUS "Using Data directory ${CCT_DATA_DIR}")
else()
  message(NOTICE "No data directory found. Set it with CCT_DATA_DIR or if you use Docker mount it at start of the container")
endif()

set(CMAKE_CXX_STANDARD 20)

# External dependencies (linked with FetchContent)
include(FetchContent)

find_package(amc CONFIG)

if(amc_FOUND)
  set(LINK_AMC FALSE)
else()
  set(LINK_AMC TRUE)

  FetchContent_Declare(
    amadeusamc
    GIT_REPOSITORY https://github.com/AmadeusITGroup/amc.git
    GIT_TAG v2.5.0
  )

  FetchContent_MakeAvailable(amadeusamc)
endif()

if(CCT_ENABLE_TESTS)
  find_package(GTest CONFIG)

  if(NOT GTest_FOUND)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )

    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
endif()

find_package(nlohmann_json CONFIG)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
  )

  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(spdlog CONFIG)
if(NOT spdlog_FOUND)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
  )

  FetchContent_MakeAvailable(spdlog)
endif()

if(CCT_BUILD_PROMETHEUS_FROM_SRC)
  # Disable Prometheus testing
  set(ENABLE_TESTING OFF)

  # Pull mode not used for the moment
  set(ENABLE_PULL OFF)

  FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.1.0
  )

  FetchContent_MakeAvailable(prometheus-cpp)

  set(CCT_ENABLE_PROMETHEUS ON)
else()
  find_package(prometheus-cpp CONFIG)

  if(prometheus-cpp_FOUND)
    message(STATUS "Linking with prometheus-cpp")

    set(CCT_ENABLE_PROMETHEUS ON)
  else()
    message(NOTICE "Unable to find local installation of prometheus-cpp, compiling without metric export support")
  endif()
endif()

# Unit Tests

#[[ Create an executable
Syntax:
add_exe(<name> src1 [src2 ...] [LIBRARIES lib1 lib2 ...] [DEFINITIONS def1 def2])
will compile an executable named <name> from source files src1 src2...
with pre-processor definitions def1 def2 (-Ddef1 -Ddef2 ... will be added to compile command)
and link against lib1 lib2 ...and libm

Examples:
add_exe(myexe src1.cpp)
add_exe(myexe src1.cpp
   LIBRARIES ${CMAKE_SOURCE_DIR}/myLib
   DEFINITIONS UNIT_TEST)
#]]
function(add_exe name)
  set(cur_var "sources")
  set(exe_sources "")
  set(exe_libraries "")
  set(exe_definitions "")
  set(exe_include_dirs "")

  foreach(arg IN LISTS ARGN)
    if(arg STREQUAL "LIBRARIES")
      set(cur_var "libraries")
    elseif(arg STREQUAL "DEFINITIONS")
      set(cur_var "definitions")
    else()
      list(APPEND exe_${cur_var} ${arg})

      if(cur_var STREQUAL "sources")
        get_filename_component(src_dir ${arg} DIRECTORY)
        list(APPEND exe_include_dirs ${src_dir})
      endif()
    endif()
  endforeach()

  add_executable(${name} ${exe_sources})
  set_target_properties(${name} PROPERTIES
    COMPILE_DEFINITIONS "${exe_definitions}"
    BUILD_RPATH "${runtime_path}")
  target_link_libraries(${name} PRIVATE ${exe_libraries})
  list(REMOVE_DUPLICATES exe_include_dirs)
  target_include_directories(${name} PRIVATE ${exe_include_dirs} ${all_includes})
endfunction()

#[[ Create a unit test
Syntax:
add_unit_test(<name> src1 [src2 ...] [LIBRARIES lib1 lib2 ...] [DEFINITIONS def1 def2])
will compile an unit test named <name> from source files src1 src2...
with pre-processor definitions def1 def2 (-Ddef1 -Ddef2 ... will be added to compile command)
and link against lib1 lib2 ... and libm

Examples:
add_unit_test(myexe src1.cpp)
add_unit_test(myexe src1.cpp DEFINITIONS UNIT_TEST)
#]]
function(add_unit_test name)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")

  if(CCT_ENABLE_TESTS)
    add_exe(${name} ${MY_UNPARSED_ARGUMENTS})
    target_link_libraries(${name} PRIVATE GTest::gtest GTest::gmock GTest::gmock_main)

    if(LINK_AMC)
      target_link_libraries(${name} PRIVATE amc::amc)
    endif()

    target_include_directories(${name} PRIVATE include)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/http-request/include)

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
      ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1 abort_on_error=1 print_stacktrace=1;\
                          LSAN_OPTIONS=detect_leaks=1 malloc_context_size=2 print_suppressions=0"
      WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY})
  endif()
endfunction()

if(MSVC)
  add_compile_options(/W4)

  # https://stackoverflow.com/questions/5004858/why-is-stdmin-failing-when-windows-h-is-included
  add_compile_definitions(NOMINMAX)
else()
  add_compile_options(-Wall -Wextra -pedantic)

  if(CCT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-sanitize-recover)
    add_link_options(-fsanitize=address -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-sanitize-recover)
    message(STATUS "Activate asan")
  endif()

  if(CCT_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY "clang-tidy")

    if(CLANG_TIDY)
      message(STATUS "Activate clang-tidy")
      set(CMAKE_CXX_CLANG_TIDY clang-tidy)
    else()
      message(FATAL_ERROR "clang-tidy executable cannot be found")
    endif()
  endif()
endif()

# Definition of project constants
add_compile_definitions("CCT_DATA_DIR=\"${CCT_DATA_DIR}\"")
add_compile_definitions("CCT_VERSION=\"${PROJECT_VERSION}\"")

if(CCT_ENABLE_PROMETHEUS)
  add_compile_definitions(CCT_ENABLE_PROMETHEUS)
endif()

# Link to sub folders CMakeLists.txt, from the lowest level to the highest level for documentation
# (beware of cyclic dependencies)
add_subdirectory(src/tech)
add_subdirectory(src/monitoring)
add_subdirectory(src/http-request)
add_subdirectory(src/objects)
add_subdirectory(src/api-objects)
add_subdirectory(src/api)
add_subdirectory(src/engine)
add_subdirectory(src/main)
