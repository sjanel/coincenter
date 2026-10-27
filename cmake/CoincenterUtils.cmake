include(${CMAKE_CURRENT_LIST_DIR}/SetProjectProperties.cmake)

function(add_coincenter_library name)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")

  if(CCT_BUILD_SHARED)
    add_library(coincenter_${name} SHARED ${MY_UNPARSED_ARGUMENTS})
  else()
    add_library(coincenter_${name} STATIC ${MY_UNPARSED_ARGUMENTS})
  endif()

  CoincenterSetProjectProperties(coincenter_${name})
endfunction()

function(add_coincenter_executable name)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 MY "${options}" "${oneValueArgs}" "${multiValueArgs}")

  add_executable(${name} ${MY_UNPARSED_ARGUMENTS})

  CoincenterSetProjectProperties(${name})
endfunction()
