cmake_minimum_required (VERSION 3.16)

project(coincenter VERSION 1.0
                   DESCRIPTION "A C++ library centralizing several crypto currencies exchanges REST API into a single all in one tool with a unified interface"
                   LANGUAGES CXX)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

##
## MAIN_PROJECT CHECK
## determine if coincenter is built as a subproject or if it is the main project
##
set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

set(CCT_ASAN_BUILD OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  # In Debug, activate asan by default
  set(CCT_ASAN_BUILD ON)
endif()

option(CCT_ENABLE_TESTS "Build the unit tests" ${MAIN_PROJECT})
option(CCT_BUILD_EXEC_INSTEAD_OF_LIB "Build an executable instead of a static library" ${MAIN_PROJECT})
option(CCT_OVERRIDE_DATA_DIR "Override Data directory from CCT_DATA_DIR (./data by default)" OFF)
option(CCT_ENABLE_ASAN "Compile with AddressSanitizer" ${CCT_ASAN_BUILD})

if (NOT CCT_OVERRIDE_DATA_DIR)
  set(CCT_DATA_DIR "${CMAKE_SOURCE_DIR}/data")
endif()

set(CMAKE_CXX_STANDARD 20)

# External dependencies (linked with FetchContent)
include(FetchContent)

FetchContent_Declare(
  amadeusamc
  GIT_REPOSITORY https://github.com/AmadeusITGroup/amc.git
  GIT_TAG        origin/master
)

FetchContent_MakeAvailable(amadeusamc)

if(CCT_ENABLE_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        origin/master
  )

  FetchContent_MakeAvailable(googletest)

  enable_testing()
endif(CCT_ENABLE_TESTS)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent.git
  GIT_TAG        origin/master
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.x
)

FetchContent_MakeAvailable(json)

FetchContent_MakeAvailable(spdlog)

include(FindCURL)
include(FindOpenSSL)

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
      if (arg STREQUAL "LIBRARIES")
         set(cur_var "libraries")
      elseif (arg STREQUAL "DEFINITIONS")
         set(cur_var "definitions")
      else()
         list(APPEND exe_${cur_var} ${arg})
         if (cur_var STREQUAL "sources")
            get_filename_component(src_dir ${arg} DIRECTORY)
            list(APPEND exe_include_dirs ${src_dir})
         endif()
      endif()
   endforeach()
   add_executable(${name} ${exe_sources})
   set_target_properties(${name} PROPERTIES
       COMPILE_DEFINITIONS "${exe_definitions}"
       BUILD_RPATH "${runtime_path}")
   target_link_libraries(${name} PRIVATE ${exe_libraries} m rt)
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
     target_link_libraries(${name} PRIVATE gtest gmock gmock_main)
     target_link_libraries(${name} PRIVATE amc::amc)
     target_include_directories(${name} PRIVATE include)
     target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/tools/include)

     add_test(NAME ${name} COMMAND ${name})
     set_tests_properties(${name} PROPERTIES
                          ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1 abort_on_error=1 print_stacktrace=1;\
                          LSAN_OPTIONS=detect_leaks=1 malloc_context_size=2 print_suppressions=0"
                        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY})
   endif(CCT_ENABLE_TESTS)
endfunction()

# Activate all warnings, in all build modes
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

if(CCT_ENABLE_ASAN)
  if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-sanitize-recover")
    message(STATUS "Activate asan")
  endif()
endif(CCT_ENABLE_ASAN)

message(STATUS "Using Data directory ${CCT_DATA_DIR}")

add_compile_definitions("CCT_DATA_PATH=\"${CCT_DATA_DIR}\"")
add_compile_definitions("CCT_VERSION=\"${PROJECT_VERSION}\"")

# Link to sub folders CMakeLists.txt
add_subdirectory(src/tools)
add_subdirectory(src/objects)
add_subdirectory(src/api)
add_subdirectory(src/engine)
