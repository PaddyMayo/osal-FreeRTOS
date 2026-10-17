# Vendor config for the GCC_ARM_CM0 build that CI runs:
#   cmake -S . -B build/cortex-m0 -C platforms/cortex-m0/config.cmake -DENABLE_UNIT_TESTS=OFF
#
# The board is the BBC micro:bit (Cortex-M0, 16 KB of RAM).
set(PORT GCC_ARM_CM0 CACHE STRING "" FORCE)

# Cross compile for a bare-metal Arm target. The compiler check runs before any
# startup code or linker script is in play, so it builds a static library.
set(CMAKE_SYSTEM_NAME               Generic           CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER                arm-none-eabi-gcc CACHE STRING "" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY    CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS "-mcpu=cortex-m0 -mthumb -ffunction-sections -fdata-sections" CACHE STRING "" FORCE)

# An executable takes its startup code and vector table from OSAL_FREERTOS_SOURCES,
# so the toolchain's own are left out. newlib-nano with the nosys stubs supplies
# the memory and string functions the compiler calls.
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections -L${CMAKE_CURRENT_LIST_DIR}/../common" CACHE STRING "" FORCE)

set(OSAL_FREERTOS_TICK_RATE_HZ       1000     CACHE STRING "" FORCE)
set(OSAL_FREERTOS_MINIMAL_STACK_SIZE 512      CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE    8192     CACHE STRING "" FORCE)
set(OSAL_FREERTOS_CPU_CLOCK_HZ       16000000 CACHE STRING "" FORCE)

# Vector table and reset handler shared by the Cortex-M platforms, and this
# board's memory map, see link.ld
set(OSAL_FREERTOS_SOURCES       "${CMAKE_CURRENT_LIST_DIR}/../common/cortex_m_startup.c;${CMAKE_CURRENT_LIST_DIR}/../common/storage-ram/bsp_storage.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
