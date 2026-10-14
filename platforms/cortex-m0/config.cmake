# Vendor config for the GCC_ARM_CM0 build that CI runs:
#   cmake -S . -B build -C platforms/cortex-m0/config.cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DENABLE_UNIT_TESTS=OFF
set(PORT GCC_ARM_CM0 CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS "-mcpu=cortex-m0 -mthumb" CACHE STRING "" FORCE)

set(OSAL_FREERTOS_TICK_RATE_HZ       1000     CACHE STRING "" FORCE)
set(OSAL_FREERTOS_MINIMAL_STACK_SIZE 512      CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE    65536    CACHE STRING "" FORCE)
set(OSAL_FREERTOS_CPU_CLOCK_HZ       48000000 CACHE STRING "" FORCE)

# Placeholders until a board is targeted, see startup.c and link.ld
set(OSAL_FREERTOS_SOURCES       "${CMAKE_CURRENT_LIST_DIR}/startup.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
