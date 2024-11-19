cmake_minimum_required(VERSION 3.15)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.25.0.0
  DESCRIPTION "A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

#
# MAIN_PROJECT CHECK
# determine if coincenter is built as a sub-project or if it is the main project
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
option(CCT_ENABLE_PROTO "Compile with protobuf support (to export data to the outside world)" ON)
option(CCT_BUILD_PROMETHEUS_FROM_SRC "Fetch and build from prometheus-cpp sources" OFF)

set(CCT_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data" CACHE PATH "Needed data directory for coincenter. Can also be overridden at runtime with this environment variable")

if(EXISTS ${CCT_DATA_DIR})
  message(STATUS "Using Data directory ${CCT_DATA_DIR}")
else()
  message(NOTICE "No data directory found. Set it with CCT_DATA_DIR or if you use Docker mount it at start of the container")
endif()

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# openssl for requests
find_package(OpenSSL REQUIRED)

# ZLIB - required for serialization data compression
find_package(ZLIB REQUIRED)

# curl - request library
find_package(CURL REQUIRED)

# External dependencies (linked with FetchContent)
include(FetchContent)

set(fetchContentPackagesToMakeAvailable "")

find_package(amc CONFIG)
if(amc_FOUND)
  set(LINK_AMC FALSE)
else()
  set(LINK_AMC TRUE)

  FetchContent_Declare(
    amadeusamc
    URL https://github.com/AmadeusITGroup/amc/archive/refs/tags/v2.5.1.tar.gz
    URL_HASH SHA256=8358c2410d95c2d2653a11221ee0ffc7debaf10d5fa1edd154a7d85df52f8209
  )

  list(APPEND fetchContentPackagesToMakeAvailable amadeusamc)
endif()

if(CCT_ENABLE_TESTS)
  find_package(GTest CONFIG)

  if(NOT GTest_FOUND)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
    )

    list(APPEND fetchContentPackagesToMakeAvailable googletest)
  endif()

  enable_testing()
endif()

# nlohmann_json - json container library
find_package(nlohmann_json CONFIG)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
  )

  list(APPEND fetchContentPackagesToMakeAvailable nlohmann_json)
endif()

# Glaze - fast json serialization library
find_package(glaze CONFIG)
if(NOT glaze)

  FetchContent_Declare(
    glaze
    URL https://github.com/stephenberry/glaze/archive/refs/tags/v4.0.1.tar.gz
    URL_HASH SHA256=0026aca33201ee6d3a820fb5926f36ba8c838bfd3120e2e179b0eee62b5bd231
  )

  list(APPEND fetchContentPackagesToMakeAvailable glaze)
endif()

# prometheus for monitoring support
if(CCT_BUILD_PROMETHEUS_FROM_SRC)
  FetchContent_Declare(
    prometheus-cpp
    URL https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.2.4.tar.gz
    URL_HASH SHA256=48dbad454d314b836cc667ec4def93ec4a6e4255fc8387c20cacb3b8b6faee30
  )

  # Disable Prometheus testing
  set(ENABLE_TESTING OFF)

  # Pull mode not used for the moment
  set(ENABLE_PULL OFF)

  list(APPEND fetchContentPackagesToMakeAvailable prometheus-cpp)

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
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.0.tar.gz
    URL_HASH SHA256=9962648c9b4f1a7bbc76fd8d9172555bad1871fdb14ff4f842ef87949682caa5
  )

  list(APPEND fetchContentPackagesToMakeAvailable spdlog)
endif()

# JWT library
FetchContent_Declare(
  jwt-cpp
  URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.0.tar.gz
  URL_HASH SHA256=b9eb270e3ba8221e4b2bc38723c9a1cb4fa6c241a42908b9a334daff31137406
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

list(APPEND fetchContentPackagesToMakeAvailable jwt-cpp)

# protobuf - serialization / deserialization library
set(PROTOBUF_FETCHED_CONTENT OFF)
if(CCT_ENABLE_PROTO)
  find_package(Protobuf CONFIG)
  if(Protobuf_FOUND)
    message(STATUS "Linking with protobuf ${protobuf_VERSION}")
  else()
    # Check here for a new version: https://protobuf.dev/support/version-support/#cpp
    if (NOT PROTOBUF_VERSION)
      set(PROTOBUF_VERSION v5.28.3)
    endif()

    message(STATUS "Configuring protobuf ${PROTOBUF_VERSION} from sources")
    
    # Using git here to simplify cmake code instead of archive download as 
    # it depends on Abseil behind the scene whose code is not included in the release archives.
    FetchContent_Declare(
      protobuf
      GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
      GIT_TAG ${PROTOBUF_VERSION}
      GIT_SHALLOW true
    )

    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBUPB OFF CACHE BOOL "" FORCE)
 
    # Abseil options
    set(ABSL_CXX_STANDARD ${CMAKE_CXX_STANDARD} CACHE STRING "" FORCE)
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

    if(CCACHE_PROGRAM)
      set(protobuf_ALLOW_CCACHE ON CACHE BOOL "" FORCE)
    endif()

    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBUPB OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE INTERNAL "")

    list(APPEND fetchContentPackagesToMakeAvailable protobuf)

    set(PROTOBUF_FETCHED_CONTENT ON)
    set(Protobuf_LIBRARIES "protobuf::libprotobuf")
  endif()
endif()

# Make fetch content available
if(fetchContentPackagesToMakeAvailable)
  message(STATUS "Configuring packages ${fetchContentPackagesToMakeAvailable}")

  FetchContent_MakeAvailable("${fetchContentPackagesToMakeAvailable}")

  if(PROTOBUF_FETCHED_CONTENT)
    include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
  endif()
endif()

# Unit Tests
include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddUnitTest.cmake)

# coincenter library factorization of target options
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CoincenterUtils.cmake)

if(MSVC)
  # https://stackoverflow.com/questions/5004858/why-is-stdmin-failing-when-windows-h-is-included
  add_compile_definitions(NOMINMAX)
  add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
  add_compile_definitions(_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)
else()
  if(CCT_ENABLE_ASAN)
    if (NOT CCT_ASAN_OPTIONS)
      set(CCT_ASAN_OPTIONS -fsanitize=address -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-sanitize-recover)
    endif()
    add_compile_options(${CCT_ASAN_OPTIONS})
    add_link_options(${CCT_ASAN_OPTIONS})

    message(STATUS "Activate asan with options ${CCT_ASAN_OPTIONS}")
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

# Link to sub folders CMakeLists.txt, from the lowest level to the highest level for documentation
# (beware of cyclic dependencies)
add_subdirectory(src/tech)
add_subdirectory(src/basic-objects)
add_subdirectory(src/schema)
add_subdirectory(src/monitoring)
add_subdirectory(src/http-request)
add_subdirectory(src/objects)
add_subdirectory(src/serialization)
add_subdirectory(src/api-objects)
add_subdirectory(src/trading)
add_subdirectory(src/api)
add_subdirectory(src/engine)
add_subdirectory(src/main)
