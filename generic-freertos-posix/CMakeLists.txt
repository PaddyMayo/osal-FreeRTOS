##########################################################################
#
# "generic-freertos-posix" BSP: FreeRTOS on the kernel's POSIX simulator
# port. The whole platform is this one declaration - see
# generic-freertos/osal_freertos_platform.cmake for every setting.
#
##########################################################################

include(${CMAKE_SOURCE_DIR}/generic-freertos/osal_freertos_platform.cmake)

osal_freertos_platform(
    PORT               GCC_POSIX
    CONSOLE            posix
    TICK_RATE_HZ       100
    MINIMAL_STACK_SIZE "PTHREAD_STACK_MIN"
    # Sized for PTHREAD_STACK_MIN-based task stacks: the OSAL startup task alone needs 4 MiB on aarch64
    TOTAL_HEAP_SIZE    "((size_t)(16 * 1024 * 1024))"
    HEADERS            pthread.h
    # The POSIX port's portmacro.h defines portCLEAN_UP_TCB to cancel the task's pthread
    PORT_CLEAN_UP_TCB  "vPortCancelThread(pxTCB)"
)
