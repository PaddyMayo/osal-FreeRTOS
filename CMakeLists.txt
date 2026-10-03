cmake_minimum_required(VERSION 3.16)
project(osal_freertos_port C)

set(FREERTOS_HEAP 4 CACHE STRING "")

set(OSAL_SYSTEM_OSTYPE freertos)
set(OSAL_SYSTEM_BSPTYPE generic-freertos-posix)
set(OSAL_EXT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "")
set(ENABLE_UNIT_TESTS TRUE CACHE BOOL "")

enable_testing()

add_subdirectory(third_party/osal osal_build)
add_subdirectory(third_party/FreeRTOS-Kernel freertos_kernel)

# osal_bsp only picks up the BSP's compiled objects via $<TARGET_OBJECTS:...>,
# which does not inherit that target's own link to freertos_kernel - so it is
# linked here, otherwise anything linking osal_bsp fails with undefined
# references to FreeRTOS task/queue/scheduler symbols.
target_link_libraries(osal_bsp PUBLIC freertos_kernel)

if (ENABLE_UNIT_TESTS)
    add_subdirectory(freertos-coverage-tests)
endif ()