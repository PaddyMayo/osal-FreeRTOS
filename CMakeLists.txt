cmake_minimum_required(VERSION 3.16)
project(osal_freertos_port C)

set(FREERTOS_PORT GCC_POSIX CACHE STRING "")
set(FREERTOS_HEAP 4 CACHE STRING "")

set(OSAL_SYSTEM_OSTYPE freertos)
set(OSAL_SYSTEM_BSPTYPE freertos-posix-sim)
set(OSAL_EXT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "")
set(ENABLE_UNIT_TESTS TRUE CACHE BOOL "")

add_subdirectory(third_party/osal osal_build)
add_subdirectory(third_party/FreeRTOS-Kernel freertos_kernel)