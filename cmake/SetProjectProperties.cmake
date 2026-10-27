# Usage: CoincenterSetProjectProperties(<target>)
#
# Applies to a coincenter target all the compile / link settings of the project that must NOT leak to the fetched
# dependencies (aeronet, protobuf, abseil, gtest...): warnings, definitions, sanitizers, clang-tidy...
#
# Settings that have to reach the dependencies too stay global, in the top CMakeLists.txt, before they are added:
#  - CCT_ENABLE_ASAN address sanitizer (a dependency built without it has a different ABI than coincenter, protobuf for
#    instance changes the layout of its generated code and of its runtime when it is defined),
#  - CCT_COMPACT_DEBUG_INFO (see cmake/CompactDebugInfo.cmake),
#  - position independent code for CCT_BUILD_SHARED, and _DARWIN_C_SOURCE for GCC on macOS.
function(CoincenterSetProjectProperties name)
  target_include_directories(${name} PUBLIC include)

  # Compiler warnings (only in Debug mode)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
      target_compile_options(${name} PRIVATE /W3)
    else()
      target_compile_options(${name} PRIVATE -Wall -Wextra -pedantic -Wshadow)
    endif()
  else()
    if(MSVC)
      target_compile_options(${name} PRIVATE /W1)
    else()
      target_compile_options(${name} PRIVATE -Wdisabled-optimization)
    endif()
  endif()

  # Definition of project constants
  target_compile_definitions(${name} PRIVATE CCT_DATA_DIR=\"${CCT_DATA_DIR}\" CCT_VERSION=\"${PROJECT_VERSION}\")

  if(CCT_ENABLE_PROMETHEUS)
    target_compile_definitions(${name} PRIVATE CCT_ENABLE_PROMETHEUS)
  endif()

  if(CCT_ENABLE_PROTO)
    target_compile_definitions(${name} PRIVATE CCT_ENABLE_PROTO CCT_PROTOBUF_VERSION=\"${PROTOBUF_VERSION}\")
  endif()

  if(MSVC)
    # https://stackoverflow.com/questions/5004858/why-is-stdmin-failing-when-windows-h-is-included
    target_compile_definitions(${name} PRIVATE
      NOMINMAX
      _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
      _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)
    target_compile_options(${name} PRIVATE /Zc:preprocessor)  # Needed by glaze
  else()
    # Address sanitizer is global (see above), here are the additional sanitizers, not required by the dependencies
    # (and some of them do not compile with them on every compiler, abseil with UBSan and GCC 13 for instance).
    if(CCT_ENABLE_ASAN)
      target_compile_options(${name} PRIVATE ${CCT_ASAN_OPTIONS})
      target_link_options(${name} PRIVATE ${CCT_ASAN_OPTIONS})
    endif()

    if(CCT_ENABLE_CLANG_TIDY)
      set_target_properties(${name} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY}")
    endif()
  endif()
endfunction()
