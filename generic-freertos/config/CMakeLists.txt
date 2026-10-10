##########################################################################
#
# "generic-freertos" BSP: one osal_freertos_platform() call per supported port,
# run only for the PORT the vendor's -C config file names. The settings and the
# config file format are in osal_freertos_platform.cmake.
#
##########################################################################

include(${CMAKE_CURRENT_LIST_DIR}/osal_freertos_platform.cmake)

# POSIX simulator: stack size defaults to the C library's minimum; the port cancels the task's pthread on delete
if ("${PORT}" STREQUAL "GCC_POSIX")
    osal_freertos_platform(
        DEFAULT_MINIMAL_STACK_SIZE PTHREAD_STACK_MIN
        HEADER                     pthread.h
        PORT_CLEAN_UP_TCB          vPortCancelThread
    )

# ARMv6-M (Cortex-M0): core clock, no interrupt priorities
elseif ("${PORT}" STREQUAL "GCC_ARM_CM0")
    osal_freertos_platform(
        CORE_CLOCK
        PORT_DEFINES configENABLE_MPU=0
    )

# ARMv7-M (Cortex-M3/M4F/M7): core clock and interrupt priorities
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM3|CM4F|CM7)$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
    )

# ARMv7-M with an MPU: needs a linker script that defines the privileged-region symbols
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM3|CM4)_MPU$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES configTOTAL_MPU_REGIONS=8 configUSE_MPU_WRAPPERS_V1=1
    )

# ARMv8-M without TrustZone: the port requires every feature switch to be stated
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM23|CM33|CM35P|CM52|CM55|CM85)_NTZ_NONSECURE$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES configENABLE_FPU=0 configENABLE_MPU=0 configENABLE_MVE=0 configENABLE_TRUSTZONE=0
    )

# ARMv8-M with TrustZone, non-secure side: the port frees the secure context on task delete
elseif ("${PORT}" MATCHES "^GCC_ARM_(CM23|CM33|CM35P|CM52|CM55|CM85)_NONSECURE$")
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES configENABLE_FPU=0 configENABLE_MPU=0 configENABLE_MVE=0 configENABLE_TRUSTZONE=1
        PORT_CLEAN_UP_TCB vPortFreeSecureContext
    )

else ()
    message(FATAL_ERROR "Unsupported PORT=${PORT}")
endif ()
