######################################################################
#
# CMAKE build recipe for FreeRTOS Board Support Package (BSP)
#
######################################################################

# The FreeRTOS port, its settings and freertos_config come from the vendor's
# -C config file, see config/osal_freertos_platform.cmake
add_subdirectory(config)

# This basic implementation library holds the startup and console code. The
# vendor's startup code and vector table (OSAL_FREERTOS_SOURCES) join it.
add_library(osal_generic-freertos_impl OBJECT
    src/bsp_start.c
    src/bsp_console.c
    ${OSAL_FREERTOS_SOURCES}
)
target_link_libraries(osal_generic-freertos_impl PUBLIC freertos_kernel)

# This BSP only works with "freertos" OS layer.
# Confirming this reduces risk of accidental misconfiguration
set_property(TARGET osal_generic-freertos_impl PROPERTY OSAL_EXPECTED_OSTYPE "freertos")
