##########################################################################
#
# "generic-freertos" BSP: one osal_freertos_platform() call per supported port,
# run only for the PORT the vendor's -C config file names. The settings and the
# config file format are in config/osal_freertos_platform.cmake.
#
##########################################################################

include(${CMAKE_CURRENT_LIST_DIR}/config/osal_freertos_platform.cmake)

# Declares a family of ports that share one declaration; PORT selects the family,
# and the list feeds the error message below
set(OSAL_FREERTOS_SUPPORTED_PORTS "")
macro(osal_freertos_port)
    set(family_ports ${ARGN})
    list(APPEND OSAL_FREERTOS_SUPPORTED_PORTS ${family_ports})
    if ("${PORT}" IN_LIST family_ports)
        set(OSAL_FREERTOS_PORT_SELECTED TRUE)
    else ()
        set(OSAL_FREERTOS_PORT_SELECTED FALSE)
    endif ()
endmacro()

osal_freertos_port(GCC_POSIX)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CONSOLE                    posix
        # Fixed by the C library, not a chip; the config file may override it
        DEFAULT_MINIMAL_STACK_SIZE PTHREAD_STACK_MIN
        HEADERS                    pthread.h
        # The port's own portCLEAN_UP_TCB cancels the task's pthread
        PORT_CLEAN_UP_TCB          "vPortCancelThread(pxTCB)"
    )
endif ()

# Cortex-M: every port takes its tick from the core clock, and all but ARMv6-M
# use interrupt priorities

# ARMv6-M: no interrupt priorities
osal_freertos_port(GCC_ARM_CM0)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CORE_CLOCK
        PORT_DEFINES                   configENABLE_MPU=0
    )
endif ()

# ARMv7-M
osal_freertos_port(GCC_ARM_CM3 GCC_ARM_CM4F GCC_ARM_CM7)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
    )
endif ()

# ARMv7-M with an MPU: needs the region count and the v1 wrappers, and a linker script that
# defines the privileged-region symbols
osal_freertos_port(GCC_ARM_CM3_MPU GCC_ARM_CM4_MPU)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES                   configTOTAL_MPU_REGIONS=8 configUSE_MPU_WRAPPERS_V1=1
    )
endif ()

# ARMv8-M without TrustZone: the port requires every feature switch to be stated
osal_freertos_port(GCC_ARM_CM23_NTZ_NONSECURE GCC_ARM_CM33_NTZ_NONSECURE GCC_ARM_CM35P_NTZ_NONSECURE
                  GCC_ARM_CM52_NTZ_NONSECURE GCC_ARM_CM55_NTZ_NONSECURE GCC_ARM_CM85_NTZ_NONSECURE)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES                   configENABLE_FPU=0 configENABLE_MPU=0 configENABLE_MVE=0 configENABLE_TRUSTZONE=0
    )
endif ()

# ARMv8-M with TrustZone, non-secure side: the port's own portCLEAN_UP_TCB frees the secure context
osal_freertos_port(GCC_ARM_CM23_NONSECURE GCC_ARM_CM33_NONSECURE GCC_ARM_CM35P_NONSECURE
                  GCC_ARM_CM52_NONSECURE GCC_ARM_CM55_NONSECURE GCC_ARM_CM85_NONSECURE)
if (OSAL_FREERTOS_PORT_SELECTED)
    osal_freertos_platform(
        CORE_CLOCK
        INTERRUPT_PRIORITIES
        PORT_DEFINES                   configENABLE_FPU=0 configENABLE_MPU=0 configENABLE_MVE=0 configENABLE_TRUSTZONE=1
        PORT_CLEAN_UP_TCB              "vPortFreeSecureContext((uint32_t *)pxTCB)"
    )
endif ()

# freertos_config exists only if a block above matched PORT
if (NOT TARGET freertos_config)
    list(JOIN OSAL_FREERTOS_SUPPORTED_PORTS ", " supported_ports)
    if ("${PORT}" STREQUAL "")
        message(FATAL_ERROR "PORT is not set - set it in the -C config file, one of: ${supported_ports}")
    endif ()
    message(FATAL_ERROR "Unsupported PORT=${PORT} - supported: ${supported_ports}")
endif ()
