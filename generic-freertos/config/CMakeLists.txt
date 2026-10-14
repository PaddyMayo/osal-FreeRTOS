##########################################################################
#
# "generic-freertos" BSP: one osal_freertos_platform() call per supported port,
# run only for the PORT the vendor's -C config file names. The settings and the
# config file format are in osal_freertos_platform.cmake.
#
# Each call also names the port's family through PORT_DEFINES, so the BSP tests
# one macro instead of guessing from the compiler: OSAL_FREERTOS_PORT_CORTEX_M
# for every Cortex-M port, OSAL_FREERTOS_PORT_POSIX for the POSIX simulator.
# OS_BSP_MaskAllInterrupts in bsp_console.c fails the build for a port that
# names neither.
#
##########################################################################

include(${CMAKE_CURRENT_LIST_DIR}/osal_freertos_platform.cmake)

# POSIX simulator: stack size defaults to the C library's minimum; the port cancels the task's pthread on delete.
# Unlike the Cortex-M ports, whose vPortEndScheduler asserts, it implements vPortEndScheduler, so the BSP
# may end the scheduler (OSAL_FREERTOS_PORT_ENDS_SCHEDULER, see OS_BSP_Exit in bsp_start.c).
if ("${PORT}" STREQUAL "GCC_POSIX")
    osal_freertos_platform(
        DEFAULT_MINIMAL_STACK_SIZE PTHREAD_STACK_MIN
        HEADER                     pthread.h
        PORT_CLEAN_UP_TCB          vPortCancelThread
        PORT_DEFINES               OSAL_FREERTOS_PORT_POSIX=1 OSAL_FREERTOS_PORT_ENDS_SCHEDULER=1
    )

# ARMv6-M (Cortex-M0): core clock, no interrupt priorities
elseif ("${PORT}" STREQUAL "GCC_ARM_CM0")
    osal_freertos_platform(
        CORE_CLOCK
        PORT_DEFINES OSAL_FREERTOS_PORT_CORTEX_M=1 configENABLE_MPU=0
    )

# ARMv7-M (Cortex-M3/M4F/M7): core clock and interrupt priorities
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM3|CM4F|CM7)$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES OSAL_FREERTOS_PORT_CORTEX_M=1
    )

# ARMv7-M with an MPU: needs a linker script that defines the privileged-region symbols
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM3|CM4)_MPU$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES OSAL_FREERTOS_PORT_CORTEX_M=1 configTOTAL_MPU_REGIONS=8 configUSE_MPU_WRAPPERS_V1=1
    )

# ARMv8-M without TrustZone: the port requires every feature switch to be stated
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM23|CM33|CM35P|CM52|CM55|CM85)_NTZ_NONSECURE$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES OSAL_FREERTOS_PORT_CORTEX_M=1 configENABLE_FPU=0 configENABLE_MPU=0 configENABLE_MVE=0 configENABLE_TRUSTZONE=0
    )

else ()
    message(FATAL_ERROR "Unsupported PORT=${PORT}")
endif ()
