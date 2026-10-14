# Vendor config for the GCC_ARM_CM4F build that CI runs:
#   cmake -S . -B build-cortex-m4f -C platforms/cortex-m4f/config.cmake -DENABLE_UNIT_TESTS=OFF
set(PORT GCC_ARM_CM4F CACHE STRING "" FORCE)

# Cross compile for a bare-metal Arm target. No startup code or linker script
# exists at compiler-check time, so the compiler check builds a static library.
set(CMAKE_SYSTEM_NAME               Generic           CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER                arm-none-eabi-gcc CACHE STRING "" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY    CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16" CACHE STRING "" FORCE)

set(OSAL_FREERTOS_TICK_RATE_HZ       1000     CACHE STRING "" FORCE)
set(OSAL_FREERTOS_MINIMAL_STACK_SIZE 512      CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE    65536    CACHE STRING "" FORCE)
set(OSAL_FREERTOS_CPU_CLOCK_HZ       48000000 CACHE STRING "" FORCE)
set(OSAL_FREERTOS_KERNEL_INTERRUPT_PRIORITY      255 CACHE STRING "" FORCE)
set(OSAL_FREERTOS_MAX_SYSCALL_INTERRUPT_PRIORITY 191 CACHE STRING "" FORCE)

# Placeholders until a board is targeted, see startup.c and link.ld
set(OSAL_FREERTOS_SOURCES       "${CMAKE_CURRENT_LIST_DIR}/startup.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
