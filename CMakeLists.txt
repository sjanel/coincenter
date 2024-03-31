cmake_minimum_required(VERSION 3.15)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.23.0
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

# openssl for requests
find_package(OpenSSL REQUIRED)

# curl - request library
find_package(CURL REQUIRED)

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

# nlohmann_json - json library
find_package(nlohmann_json CONFIG)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )

  FetchContent_MakeAvailable(nlohmann_json)
endif()

# prometheus for monitoring support
if(CCT_BUILD_PROMETHEUS_FROM_SRC)
  # Disable Prometheus testing
  set(ENABLE_TESTING OFF)

  # Pull mode not used for the moment
  set(ENABLE_PULL OFF)

  FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4
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

# spdlog - logging library
find_package(spdlog CONFIG)
if(NOT spdlog_FOUND)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
  )

  FetchContent_MakeAvailable(spdlog)
endif()

# Unit Tests
include(cmake/AddUnitTest.cmake)

if(MSVC)
  add_compile_options(/W3)

  # https://stackoverflow.com/questions/5004858/why-is-stdmin-failing-when-windows-h-is-included
  add_compile_definitions(NOMINMAX)
  add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
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
