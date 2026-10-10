##########################################################################
#
# osal_freertos_platform(): the one declaration a FreeRTOS BSP makes, for the
# PORT named in the vendor's -C config file. It creates freertos_config, which
# carries every value FreeRTOSConfig.h needs as a compile definition.
#
# Arguments, set per port in generic-freertos/config/CMakeLists.txt:
#
#   CORE_CLOCK                  the port takes OSAL_FREERTOS_CPU_CLOCK_HZ
#   INTERRUPT_PRIORITIES        the port takes the two interrupt priority keys
#   PORT_DEFINES                NAME=VALUE pairs the port's portmacro.h requires
#   PORT_CLEAN_UP_TCB           cleanup function taking the TCB as void *
#   HEADER                      system header the port's values need
#   DEFAULT_MINIMAL_STACK_SIZE  stack size when the config file sets none
#
# Config file, applied with cmake -C <file>. Every set() needs CACHE STRING ""
# FORCE, since the values only reach the build through the cache. A key the
# port does not take, a missing key and an unknown OSAL_FREERTOS_ name are
# rejected. Use a fresh build directory when switching configs.
#
#   set(PORT                                         <port>          CACHE STRING "" FORCE)  # FreeRTOS port name
#   set(OSAL_FREERTOS_TICK_RATE_HZ                   <hz>            CACHE STRING "" FORCE)  # tick rate in Hz
#   set(OSAL_FREERTOS_MINIMAL_STACK_SIZE             <bytes>         CACHE STRING "" FORCE)  # idle task stack, in bytes
#   set(OSAL_FREERTOS_TOTAL_HEAP_SIZE                <bytes>         CACHE STRING "" FORCE)  # heap, in bytes
#   set(OSAL_FREERTOS_CPU_CLOCK_HZ                   <hz>            CACHE STRING "" FORCE)  # only with CORE_CLOCK
#   set(OSAL_FREERTOS_KERNEL_INTERRUPT_PRIORITY      <c-expr>        CACHE STRING "" FORCE)  # only with INTERRUPT_PRIORITIES
#   set(OSAL_FREERTOS_MAX_SYSCALL_INTERRUPT_PRIORITY <c-expr>        CACHE STRING "" FORCE)  # only with INTERRUPT_PRIORITIES
#   set(OSAL_FREERTOS_SOURCES                        <path>[;<path>] CACHE STRING "" FORCE)  # startup code, vector table
#   set(OSAL_FREERTOS_LINKER_SCRIPT                  <path>          CACHE STRING "" FORCE)  # absolute path
#
##########################################################################

set(OSAL_FREERTOS_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})

function(osal_freertos_platform)
    cmake_parse_arguments(PLATFORM
        "CORE_CLOCK;INTERRUPT_PRIORITIES"
        "HEADER;PORT_CLEAN_UP_TCB;DEFAULT_MINIMAL_STACK_SIZE"
        "PORT_DEFINES"
        ${ARGN})

    set(config_doc "${OSAL_FREERTOS_PLATFORM_DIR}/osal_freertos_platform.cmake")

    if (PLATFORM_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "osal_freertos_platform: unknown arguments: ${PLATFORM_UNPARSED_ARGUMENTS}")
    endif ()
    if ("${PORT}" STREQUAL "")
        message(FATAL_ERROR "PORT is not set - set it in the -C config file, see ${config_doc}")
    endif ()

    # ---- Keys this port takes ----------------------------------------------
    set(keys TICK_RATE_HZ MINIMAL_STACK_SIZE TOTAL_HEAP_SIZE SOURCES LINKER_SCRIPT)
    if (PLATFORM_CORE_CLOCK)
        list(APPEND keys CPU_CLOCK_HZ)
    endif ()
    if (PLATFORM_INTERRUPT_PRIORITIES)
        list(APPEND keys KERNEL_INTERRUPT_PRIORITY MAX_SYSCALL_INTERRUPT_PRIORITY)
    endif ()

    # ---- Reject unknown or unused cache entries ----------------------------
    get_cmake_property(cache_variables CACHE_VARIABLES)
    foreach (variable IN LISTS cache_variables)
        string(REGEX REPLACE "^OSAL_FREERTOS_" "" key "${variable}")
        if (key STREQUAL variable OR key IN_LIST keys OR key STREQUAL "STORAGE_TARGET")
            continue()
        endif ()
        message(FATAL_ERROR "${variable} is not a key of PORT=${PORT}, which takes: ${keys} - see ${config_doc}")
    endforeach ()

    # ---- Collect compile definitions ---------------------------------------
    # Every key is required, bar a declared default. SOURCES and LINKER_SCRIPT
    # are build inputs; the rest reach FreeRTOSConfig.h as OSAL_FREERTOS_<key>.
    set(definitions ${PLATFORM_PORT_DEFINES})
    foreach (key IN LISTS keys)
        set(value "${OSAL_FREERTOS_${key}}")
        if ("${value}" STREQUAL "")
            set(value "${PLATFORM_DEFAULT_${key}}")
        endif ()
        if ("${value}" STREQUAL "")
            message(FATAL_ERROR "OSAL_FREERTOS_${key} is required for PORT=${PORT} - set it in the -C config file, see ${config_doc}")
        endif ()
        if (NOT key MATCHES "^(SOURCES|LINKER_SCRIPT)$")
            list(APPEND definitions "OSAL_FREERTOS_${key}=${value}")
        endif ()
    endforeach ()
    if (PLATFORM_HEADER)
        list(APPEND definitions "OSAL_FREERTOS_PORT_HEADER=\"${PLATFORM_HEADER}\"")
    endif ()
    if (PLATFORM_PORT_CLEAN_UP_TCB)
        list(APPEND definitions "OSAL_FREERTOS_PORT_CLEAN_UP_TCB=${PLATFORM_PORT_CLEAN_UP_TCB}")
    endif ()

    # ---- freertos_config: read by the kernel, so it reaches every executable
    # The BSP is authoritative for the port, so FREERTOS_PORT overrides any cached value
    set(FREERTOS_PORT ${PORT} CACHE STRING "FreeRTOS port name, set by the BSP" FORCE)

    get_filename_component(linker_script "${OSAL_FREERTOS_LINKER_SCRIPT}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if (NOT EXISTS "${linker_script}")
        message(FATAL_ERROR "OSAL_FREERTOS_LINKER_SCRIPT ${linker_script} does not exist")
    endif ()

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE ${OSAL_FREERTOS_PLATFORM_DIR})
    target_compile_definitions(freertos_config INTERFACE ${definitions})
    target_link_options(freertos_config INTERFACE -T${linker_script})
endfunction()
