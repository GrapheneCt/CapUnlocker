cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
  endif()
endif()

set(PLUGIN_PROJECT CapUnlocker)
project(${PLUGIN_PROJECT})
include("${DOLCESDK}/share/dolce.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(${PLUGIN_PROJECT}
  main.c
)

target_link_libraries(${PLUGIN_PROJECT}
  taihenForKernel_stub
  SceSysclibForDriver_stub
)

set_target_properties(${PLUGIN_PROJECT}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

dolce_create_self(CapUnlocker.skprx ${PLUGIN_PROJECT}
  CONFIG ${CMAKE_SOURCE_DIR}/CapUnlocker.yml
)
