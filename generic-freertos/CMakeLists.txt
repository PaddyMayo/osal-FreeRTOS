######################################################################
#
# CMAKE build recipe for FreeRTOS Board Support Package (BSP)
#
######################################################################

# The FreeRTOS port, its settings and freertos_config come from the vendor's
# -C config file, see config/osal_freertos_platform.cmake
add_subdirectory(config)

# This basic implementation library holds the startup and console code. The
# vendor's startup code, vector table and storage device (OSAL_FREERTOS_SOURCES) join it.
add_library(osal_generic-freertos_impl OBJECT
    src/bsp_start.c
    src/bsp_console.c
    ${OSAL_FREERTOS_SOURCES}
)
target_link_libraries(osal_generic-freertos_impl PUBLIC freertos_kernel)

# Reads OSAL_FREERTOS_SOURCES for bsp_storage.c and adds its directory, where bsp_storage.h is, to the include path
set(storage_source_dir)
foreach (source IN LISTS OSAL_FREERTOS_SOURCES)
    get_filename_component(name "${source}" NAME)
    if (name STREQUAL "bsp_storage.c")
        get_filename_component(storage_source_dir "${source}" DIRECTORY)
    endif ()
endforeach ()

if (NOT storage_source_dir)
    message(FATAL_ERROR "bsp_storage.c doesn't exist for bsp")
endif ()
if (NOT EXISTS "${storage_source_dir}/bsp_storage.h")
    message(FATAL_ERROR "bsp_storage.h doesn't exist for bsp")
endif ()

target_include_directories(osal_generic-freertos_impl PRIVATE ${storage_source_dir})
# lfs.h, for the littlefs configuration the BSP fills
target_link_libraries(osal_generic-freertos_impl PRIVATE osal_littlefs)

# This BSP only works with "freertos" OS layer.
# Confirming this reduces risk of accidental misconfiguration
set_property(TARGET osal_generic-freertos_impl PROPERTY OSAL_EXPECTED_OSTYPE "freertos")
