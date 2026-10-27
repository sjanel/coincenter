cmake_minimum_required(VERSION 3.28)

# cmake does not currently support version suffixes (for dev / rc releases for instance.)
# Github actions will detect if version has 'tweak' value (3 '.' in version) 
# in order to avoid pushing Docker image for versions in development.
project(coincenter VERSION 3.27.0.0
  DESCRIPTION "A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

# coincenter does not use C++ named modules: do not scan its sources for them (CMake >= 3.28 does it by default for
# C++20 and above), which saves a scan step per source. It also keeps GCC from compiling with -fmodules-ts, under which
# __has_feature(modules) is true and the macOS SDK headers take a clang only path (rsize_t undefined in <string.h>).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(CCT_ENABLE_CCACHE "Use ccache if available (its cache lives on disk: disable it for throwaway builds in RAM)" ON)

# Use ccache if available
if(CCT_ENABLE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
endif()

if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

option(CCT_ENABLE_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(CCT_BUILD_EXEC "Build an executable instead of a static library" ${PROJECT_IS_TOP_LEVEL})
option(CCT_ENABLE_ASAN "Compile with AddressSanitizer" OFF)
option(CCT_ENABLE_CLANG_TIDY "Compile the coincenter targets (not the dependencies) with clang-tidy checks" OFF)
option(CCT_ENABLE_PROTO "Compile with protobuf support (to export data)" ON)
option(CCT_BUILD_PROMETHEUS_FROM_SRC "Fetch and build from prometheus-cpp sources" OFF)
option(CCT_BUILD_SHARED "Build coincenter (and aeronet) libraries as shared instead of static (not supported on Windows)" OFF)

# Split + compressed debug info (see cmake/CompactDebugInfo.cmake). Only on by default for Debug test builds:
# installed artifacts would reference .dwo files left in the build tree.
set(CCT_COMPACT_DEBUG_INFO_DEFAULT OFF)
if(CCT_ENABLE_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CCT_COMPACT_DEBUG_INFO_DEFAULT ON)
endif()
option(CCT_COMPACT_DEBUG_INFO
  "Split DWARF + compressed debug sections, to shrink the binaries (on by default for Debug test builds)"
  ${CCT_COMPACT_DEBUG_INFO_DEFAULT})

# Fetched dependencies are built as static libraries by default. Declaring this option here also prevents
# a dependency (brotli for instance) from defining it as ON in the cache, which would silently build all
# the dependencies configured after it (abseil...) as shared libraries.
option(BUILD_SHARED_LIBS "Build fetched dependencies as shared libraries" OFF)

if(CCT_BUILD_SHARED)
  if(WIN32)
    message(FATAL_ERROR "CCT_BUILD_SHARED is not supported on Windows (the libraries do not export their symbols)")
  endif()

  # Static dependencies (aeronet, protobuf, abseil...) are linked into the coincenter shared libraries.
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  # Do not relink the executables (tests) when only the implementation of a shared library changed:
  # editing one library source then only relinks that library.
  set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
endif()

# Set before the dependencies are added: the compile options are inherited by the sub directories added afterwards.
if(CCT_COMPACT_DEBUG_INFO)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/CompactDebugInfo.cmake)
endif()

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

if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # libstdc++ <cstdlib> expects the full Darwin namespace (at_quick_exit, quick_exit...), which the SDK hides as soon as
  # a source requests a strict POSIX level (protobuf >= v36 defines _POSIX_C_SOURCE in zero_copy_stream_impl.cc).
  # Defined before fetched dependencies are added so that it applies to them as well.
  add_compile_definitions(_DARWIN_C_SOURCE)
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
    URL https://github.com/AmadeusITGroup/amc/archive/refs/tags/v2.7.0.tar.gz
    URL_HASH SHA256=a1a261fb8ac5b8c21b57ac080874a0103a84e75fa9f59969c8366ec9b43bd8bd
    OVERRIDE_FIND_PACKAGE
    SYSTEM
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
      OVERRIDE_FIND_PACKAGE
      SYSTEM
    )

    list(APPEND fetchContentPackagesToMakeAvailable googletest)
  endif()

  enable_testing()
endif()

# Glaze - fast json serialization library
find_package(glaze CONFIG)
if(NOT glaze)

  # Kept in sync with the glaze version pinned by aeronet (they share a single glaze::glaze target
  # via FetchContent - the first declaration wins, so this one is the version aeronet is built with).
  FetchContent_Declare(
    glaze
    URL https://github.com/stephenberry/glaze/archive/refs/tags/v9.0.0.tar.gz
    URL_HASH SHA256=dd7b033c5bf6e4308615bb7834c541291d5b20f64121d9a68222a39414635517
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND fetchContentPackagesToMakeAvailable glaze)
endif()

# prometheus for monitoring support
if(CCT_BUILD_PROMETHEUS_FROM_SRC)
  FetchContent_Declare(
    prometheus-cpp
    URL https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.3.0.tar.gz
    URL_HASH SHA256=ac6e958405a29fbbea9db70b00fa3c420e16ad32e1baf941ab233ba031dd72ee
    SYSTEM
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
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND fetchContentPackagesToMakeAvailable spdlog)
endif()

# aeronet - HTTP client (replaces libcurl) and JWT / JWS support (replaces jwt-cpp).
FetchContent_Declare(
  aeronet
  URL https://github.com/sjanel/aeronet/archive/refs/tags/v1.7.0.tar.gz
  URL_HASH SHA256=4b633f2a4cfbe9b41ded749e369416a42c94da3f688f622b3e759ecafa2aee40
  OVERRIDE_FIND_PACKAGE
)

# As a sub-project aeronet keeps most features OFF (they key off PROJECT_IS_TOP_LEVEL) and JWT is a
# dependent option requiring OpenSSL + glaze. Force the features coincenter needs and disable the rest.
set(AERONET_ENABLE_HTTP_CLIENT ON  CACHE BOOL "" FORCE)  # the HTTP client we migrate to
set(AERONET_ENABLE_HTTP_SERVER ${CCT_ENABLE_TESTS} CACHE BOOL "" FORCE)  # only needed by the httpclient unit test
set(AERONET_ENABLE_ASYNC_HANDLERS OFF CACHE BOOL "" FORCE)  # server-side feature, not used
set(AERONET_ENABLE_OPENSSL     ON  CACHE BOOL "" FORCE)  # HTTPS + JWT crypto
set(AERONET_ENABLE_GLAZE       ON  CACHE BOOL "" FORCE)  # required by JWT
set(AERONET_ENABLE_JWT         ON  CACHE BOOL "" FORCE)  # replaces jwt-cpp
set(AERONET_ENABLE_ZLIB        ON  CACHE BOOL "" FORCE)  # gzip / deflate auto-decompression
set(AERONET_ENABLE_ZLIBNG      ON  CACHE BOOL "" FORCE)  # use aeronet's bundled zlib-ng (faster, self-contained)
set(AERONET_ENABLE_ZSTD        ON  CACHE BOOL "" FORCE)  # zstd auto-decompression (more efficient)
set(AERONET_ENABLE_BROTLI      ON  CACHE BOOL "" FORCE)  # brotli auto-decompression (more efficient)
set(AERONET_ENABLE_SPDLOG      ON  CACHE BOOL "" FORCE)  # enable spdlog support
set(AERONET_SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)  # spdlog is used with fmt, not std::format
set(BROTLI_BUILD_TOOLS         OFF CACHE BOOL "" FORCE)  # brotli command line tool is not needed
set(AERONET_ENABLE_HTTP2       ON CACHE BOOL "" FORCE)
set(AERONET_ENABLE_WEBSOCKET   ON CACHE BOOL "" FORCE)
set(AERONET_BUILD_TESTS        OFF CACHE BOOL "" FORCE)
set(AERONET_BUILD_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(AERONET_BUILD_BENCHMARKS   OFF CACHE BOOL "" FORCE)
set(AERONET_INSTALL            OFF CACHE BOOL "" FORCE)
set(AERONET_ENABLE_ASAN        ${CCT_ENABLE_ASAN} CACHE BOOL "" FORCE)
set(AERONET_BUILD_SHARED       ${CCT_BUILD_SHARED} CACHE BOOL "" FORCE)
set(AERONET_COMPACT_DEBUG_INFO OFF CACHE BOOL "" FORCE)  # handled for the whole tree by CCT_COMPACT_DEBUG_INFO

list(APPEND fetchContentPackagesToMakeAvailable aeronet)

# protobuf - serialization / deserialization library
set(PROTOBUF_FETCHED_CONTENT OFF)
if(CCT_ENABLE_PROTO)
  find_package(Protobuf CONFIG)
  if(Protobuf_FOUND)
    message(STATUS "Linking with protobuf ${protobuf_VERSION}")
  else()
    # Check here for a new version: https://protobuf.dev/support/version-support/#cpp
    # The git tags are named after the minor version only (the C++ 7.36.x releases are tagged v36.x).
    if (NOT PROTOBUF_VERSION)
      set(PROTOBUF_VERSION v36.2)
    endif()

    message(STATUS "Configuring protobuf ${PROTOBUF_VERSION} from sources")
    
    # EXCLUDE_FROM_ALL (here and for absl): only the targets we link or need to generate code (libprotobuf, protoc
    # and their abseil dependencies) are built, not the rest (libprotobuf-lite, upb plugins, unused abseil libraries...).
    # Using git here to simplify cmake code instead of archive download as 
    # it depends on Abseil behind the scene whose code is not included in the release archives.
    FetchContent_Declare(
      protobuf
      GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
      GIT_TAG ${PROTOBUF_VERSION}
      GIT_SHALLOW true
      SYSTEM
      EXCLUDE_FROM_ALL
    )

    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBUPB ON CACHE BOOL "" FORCE)  # forced ON by protobuf since v36 (warns otherwise)
 
    # Abseil is declared here, before protobuf, so that it is newer than the one pinned by protobuf (20250512.1):
    # protobuf only fetches its own when no absl target exists.
    FetchContent_Declare(
      absl
      URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20260817.0.tar.gz
      URL_HASH SHA256=f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d
      SYSTEM
      EXCLUDE_FROM_ALL
    )

    list(APPEND fetchContentPackagesToMakeAvailable absl)

    # Abseil options
    set(ABSL_CXX_STANDARD ${CMAKE_CXX_STANDARD} CACHE STRING "" FORCE)
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

    if(CCACHE_PROGRAM)
      set(protobuf_ALLOW_CCACHE ON CACHE BOOL "" FORCE)
    endif()

    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE INTERNAL "")

    list(APPEND fetchContentPackagesToMakeAvailable protobuf)

    set(PROTOBUF_FETCHED_CONTENT ON)
  endif()
endif()

# The dependencies must be built with AddressSanitizer too, not only coincenter (see below for the other sanitizers):
# protobuf (like abseil) changes the layout of its generated code and of its runtime when ABSL_HAVE_ADDRESS_SANITIZER
# is defined, so a libprotobuf built without it crashes with generated code built with it.
# Only the address sanitizer here, as the other ones are not required and some dependencies do not compile with them
# on every compiler (abseil with UBSan and GCC 13).
if(CCT_ENABLE_ASAN AND NOT MSVC)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

# Make fetch content available
if(fetchContentPackagesToMakeAvailable)
  message(STATUS "Configuring packages ${fetchContentPackagesToMakeAvailable}")

  # Dependencies are not ours to fix: silence their CMake deprecation warnings
  # (protobuf for instance explicitly sets some policies to OLD).
  cmake_policy(PUSH)
  if(POLICY CMP0218)
    # CMake >= 4.4: dependencies still rely on the CMAKE_WARN_DEPRECATED variable for their deprecation messages.
    cmake_policy(SET CMP0218 OLD)
  endif()
  set(CMAKE_WARN_DEPRECATED OFF)

  FetchContent_MakeAvailable("${fetchContentPackagesToMakeAvailable}")

  unset(CMAKE_WARN_DEPRECATED)
  cmake_policy(POP)

  # aeronet only takes the include directories of spdlog, so its sources compile spdlog in header-only mode while
  # coincenter links the compiled library (ODR violation): in optimized builds aeronet then logs through its own spdlog
  # registry, bypassing the coincenter loggers and used after its destruction at exit (crash with musl), and MSVC misses
  # /utf-8. All aeronet libraries link aeronet_tech, which owns aeronet/log.hpp: give it the real spdlog target.
  target_link_libraries(aeronet_tech PUBLIC spdlog::spdlog)

  if(PROTOBUF_FETCHED_CONTENT)
    include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
  endif()
endif()

# Unit Tests
include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddUnitTest.cmake)

# coincenter library factorization of target options
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CoincenterUtils.cmake)

# Project settings applied to the coincenter targets only, not to the dependencies added above:
# see CoincenterSetProjectProperties in cmake/SetProjectProperties.cmake. The variables checked there are set here.
if(NOT MSVC)
  if(CCT_ENABLE_ASAN)
    # The address sanitizer alone is already activated for the dependencies (see above)
    if(NOT CCT_ASAN_OPTIONS)
      set(CCT_ASAN_OPTIONS -fsanitize=address -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-sanitize-recover)
    endif()

    message(STATUS "Activate asan with options ${CCT_ASAN_OPTIONS}")
  endif()

  if(CCT_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY "clang-tidy")

    if(CLANG_TIDY)
      message(STATUS "Activate clang-tidy on the coincenter targets")
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
