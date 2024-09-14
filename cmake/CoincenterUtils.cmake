function(add_coincenter_library name)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")

  add_library(coincenter_${name} STATIC ${MY_UNPARSED_ARGUMENTS})

  target_set_coincenter_options(coincenter_${name})
endfunction()

function(add_coincenter_executable name)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")

  add_executable(${name} ${MY_UNPARSED_ARGUMENTS})

  target_set_coincenter_options(${name})
endfunction()

function (target_set_coincenter_options name)
  target_include_directories(${name} PUBLIC include)

  # Compiler warnings (only in Debug mode)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
      target_compile_options(${name} PRIVATE /W3)
    else()
      target_compile_options(${name} PRIVATE -Wall -Wextra -pedantic -Wshadow -Winline)
    endif()
  else()
    if(MSVC)
      
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
endfunction()