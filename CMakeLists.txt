cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(CapUnlocker C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -Wextra -Wno-unused-parameter -O2 -nostdlib -fno-builtin")

add_executable(CapUnlocker
  main.c
  capu_logic.c
)

target_link_libraries(CapUnlocker
  taihenForKernel_stub
  taihenModuleUtils_stub
  SceDebugForDriver_stub
  SceIofilemgrForDriver_stub
  SceSysclibForDriver_stub
  SceSysrootForDriver_stub
)

vita_create_self(CapUnlocker.skprx CapUnlocker CONFIG exports.yml UNSAFE)
