cmake_minimum_required(VERSION 3.16)
project(port_helpers C)

set(PH_SDL_SOURCE_DIR "" CACHE PATH "Optional SDL3 source tree when SDL3 is not installed")
if(NOT TARGET SDL3::SDL3)
  if(PH_SDL_SOURCE_DIR)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PH_SDL_SOURCE_DIR}" SDL3 EXCLUDE_FROM_ALL)
  else()
    find_package(SDL3 REQUIRED CONFIG)
  endif()
endif()

add_library(port_helpers INTERFACE)
target_include_directories(port_helpers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(port_helpers INTERFACE SDL3::SDL3)
if(WIN32)
  target_link_libraries(port_helpers INTERFACE ws2_32 iphlpapi)
endif()
if(IOS)
  target_link_libraries(port_helpers INTERFACE "-framework UIKit" "-framework UniformTypeIdentifiers")
  target_sources(port_helpers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ph_ios.m)
  target_compile_options(port_helpers INTERFACE -fobjc-arc)
endif()

set(PH_EMBED_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.cmake CACHE INTERNAL "")

function(ph_embed target name input)
  set(output ${CMAKE_CURRENT_BINARY_DIR}/${name}.c)
  add_custom_command(OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${input} -DOUTPUT=${output} -DNAME=${name} -P ${PH_EMBED_SCRIPT}
    DEPENDS ${input} ${PH_EMBED_SCRIPT}
    VERBATIM)
  add_library(${target} STATIC ${output})
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CMAKE_C_STANDARD 11)
  enable_testing()

  add_executable(ph_test tests/test.c)
  target_link_libraries(ph_test PRIVATE port_helpers)
  add_test(NAME ph_test COMMAND ph_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_executable(ph_demo demo/demo.c)
  target_link_libraries(ph_demo PRIVATE port_helpers)
endif()
