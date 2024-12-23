cmake_minimum_required(VERSION 3.15)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.25.0
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
    URL https://github.com/AmadeusITGroup/amc/archive/refs/tags/v2.5.2.tar.gz
    URL_HASH SHA256=91c092f7add31493e0071eef49103c96f03611ca476b844278175459d0679335
  )

  list(APPEND fetchContentPackagesToMakeAvailable amadeusamc)
endif()

if(CCT_ENABLE_TESTS)
  find_package(GTest CONFIG)

  if(NOT GTest_FOUND)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.tar.gz
      URL_HASH SHA256=78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399
    )

    list(APPEND fetchContentPackagesToMakeAvailable googletest)
  endif()

  enable_testing()
endif()

# Glaze - fast json serialization library
find_package(glaze CONFIG)
if(NOT glaze)

  FetchContent_Declare(
    glaze
    URL https://github.com/stephenberry/glaze/archive/refs/tags/v5.1.0.tar.gz
    URL_HASH SHA256=c91265728918f914a69a935e2d1dbca26d025170b6d338220fc83b698c913f80
  )

  list(APPEND fetchContentPackagesToMakeAvailable glaze)
endif()

# prometheus for monitoring support
if(CCT_BUILD_PROMETHEUS_FROM_SRC)
  FetchContent_Declare(
    prometheus-cpp
    URL https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.3.0.tar.gz
    URL_HASH SHA256=ac6e958405a29fbbea9db70b00fa3c420e16ad32e1baf941ab233ba031dd72ee
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
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7a80896357f3e8e920e85e92633b14ba0f229c506e6f978578bdc35ba09e9a5d
  )

  list(APPEND fetchContentPackagesToMakeAvailable spdlog)
endif()

# JWT library
FetchContent_Declare(
  jwt-cpp
  URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.1.tar.gz
  URL_HASH SHA256=e52f247d5e62fac5da6191170998271a70ce27f747f2ce8fde9b09f96a5375a4
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
      set(PROTOBUF_VERSION v6.30.2)
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
  add_compile_options(/Zc:preprocessor) # Needed by glaze
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
