add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE inc)

add_library(osal_freertos-posix-sim_impl OBJECT
    src/bsp_start.c
)
target_include_directories(osal_freertos-posix-sim_impl PUBLIC inc)