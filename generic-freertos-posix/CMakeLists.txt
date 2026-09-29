add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE OSAL_FREERTOS_PORT_POSIX)

add_library(osal_generic-freertos-posix_impl OBJECT
    src/bsp_console.c
    ${CMAKE_SOURCE_DIR}/generic-freertos/src/bsp_start.c
)
target_include_directories(osal_generic-freertos-posix_impl PUBLIC inc)
target_link_libraries(osal_generic-freertos-posix_impl PUBLIC freertos_kernel)