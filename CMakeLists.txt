cmake_minimum_required(VERSION 3.28)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.26.0.0
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_program(CLANG_TIDY "clang-tidy")
endif()

option(CCT_ENABLE_TESTS "Build the unit tests" ${MAIN_PROJECT})
option(CCT_BUILD_EXEC "Build an executable instead of a static library" ${MAIN_PROJECT})
option(CCT_ENABLE_ASAN "Compile with AddressSanitizer" OFF)
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

# ZLIB - only required by protobuf for serialization data compression.
# aeronet no longer needs the system zlib: it builds its own bundled zlib-ng (see AERONET_ENABLE_ZLIBNG below).
if(CCT_ENABLE_PROTO)
  find_package(ZLIB REQUIRED)
endif()

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
    URL https://github.com/AmadeusITGroup/amc/archive/refs/tags/v2.6.1.tar.gz
    URL_HASH SHA256=a1cbb695f31c96b90699ef1d2db14dcd89984cf1135c29b48af70a007c44a02d
  )

  list(APPEND fetchContentPackagesToMakeAvailable amadeusamc)
endif()

if(CCT_ENABLE_TESTS)
  find_package(GTest CONFIG)

  if(NOT GTest_FOUND)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
      URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    )

    list(APPEND fetchContentPackagesToMakeAvailable googletest)
  endif()

  enable_testing()
endif()

# Glaze - fast json serialization library
find_package(glaze CONFIG)
if(NOT glaze)

  # Kept in sync with the glaze version pinned by aeronet (they share a single glaze::glaze target
  # via FetchContent, and aeronet requires glaze >= 7 for its <glaze/yaml.hpp> config loader).
  FetchContent_Declare(
    glaze
    URL https://github.com/stephenberry/glaze/archive/refs/tags/v7.7.1.tar.gz
    URL_HASH SHA256=c51217ca374f2a7d556c501a849d3a43dfffff47ba7711b6f0c22020e71ff63d
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
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
  )

  list(APPEND fetchContentPackagesToMakeAvailable spdlog)
endif()

# aeronet - HTTP client (replaces libcurl) and JWT / JWS support (replaces jwt-cpp).
FetchContent_Declare(
  aeronet
    URL https://github.com/sjanel/aeronet/archive/refs/tags/v1.5.0.tar.gz
    URL_HASH SHA256=109485da8ca0b1c242d3b7a3daf17d4e15183bcc58ffd3da2ea83d77b98191bd
)

# As a sub-project aeronet keeps most features OFF (they key off PROJECT_IS_TOP_LEVEL) and JWT is a
# dependent option requiring OpenSSL + glaze. Force the features coincenter needs and disable the rest.
set(AERONET_ENABLE_HTTP_CLIENT ON  CACHE BOOL "" FORCE)  # the HTTP client we migrate to
set(AERONET_ENABLE_OPENSSL     ON  CACHE BOOL "" FORCE)  # HTTPS + JWT crypto
set(AERONET_ENABLE_GLAZE       ON  CACHE BOOL "" FORCE)  # required by JWT
set(AERONET_ENABLE_JWT         ON  CACHE BOOL "" FORCE)  # replaces jwt-cpp
set(AERONET_ENABLE_ZLIB        ON  CACHE BOOL "" FORCE)  # gzip / deflate auto-decompression
set(AERONET_ENABLE_ZLIBNG      ON  CACHE BOOL "" FORCE)  # use aeronet's bundled zlib-ng (faster, self-contained)
set(AERONET_ENABLE_ZSTD        ON  CACHE BOOL "" FORCE)  # zstd auto-decompression (more efficient)
set(AERONET_ENABLE_BROTLI      ON  CACHE BOOL "" FORCE)  # brotli auto-decompression (more efficient)
set(AERONET_ENABLE_HTTP2       OFF CACHE BOOL "" FORCE)  # HTTP/1.1 is enough; fewer dependencies
set(AERONET_ENABLE_WEBSOCKET   OFF CACHE BOOL "" FORCE)
set(AERONET_BUILD_TESTS        OFF CACHE BOOL "" FORCE)
set(AERONET_BUILD_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(AERONET_BUILD_BENCHMARKS   OFF CACHE BOOL "" FORCE)
set(AERONET_INSTALL            OFF CACHE BOOL "" FORCE)
set(AERONET_ENABLE_ASAN        ${CCT_ENABLE_ASAN} CACHE BOOL "" FORCE)

list(APPEND fetchContentPackagesToMakeAvailable aeronet)

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
