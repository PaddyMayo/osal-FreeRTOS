##########################################################################
#
# osal_freertos_platform(): the one declaration a FreeRTOS BSP makes.
#
# A BSP's CMakeLists.txt is a single call to this function, which turns it
# into the generated FreeRTOSConfigPlatform.h, the freertos_config target the
# kernel reads it through, and the osal_<bsp>_impl target OSAL builds the BSP
# from. The keys below are the platform contract; nothing else states it.
#
#   Required:
#     PORT                FreeRTOS-Kernel port name (FREERTOS_PORT), e.g. GCC_POSIX, GCC_ARM_CM4F
#     CONSOLE             console backend: generic-freertos/console/<CONSOLE>.h
#     TICK_RATE_HZ        -> configTICK_RATE_HZ
#     MINIMAL_STACK_SIZE  -> configMINIMAL_STACK_SIZE, in StackType_t words
#     TOTAL_HEAP_SIZE     -> configTOTAL_HEAP_SIZE, in bytes
#
#   Optional:
#     CPU_CLOCK_HZ                    -> configCPU_CLOCK_HZ
#     KERNEL_INTERRUPT_PRIORITY       -> configKERNEL_INTERRUPT_PRIORITY
#     MAX_SYSCALL_INTERRUPT_PRIORITY  -> configMAX_SYSCALL_INTERRUPT_PRIORITY
#     OPTIMISED_TASK_SELECTION        -> configUSE_PORT_OPTIMISED_TASK_SELECTION (default 0)
#     PORT_CLEAN_UP_TCB               the port's own TCB cleanup, as an expression of pxTCB; set
#                                     only when the port's portmacro.h already defines portCLEAN_UP_TCB
#     HEADERS                         system headers the values above need
#     CONSOLE_DEFINES                 compile definitions the console backend needs
#     SOURCES                         extra BSP sources (startup code, vector table, ...)
#     LINKER_SCRIPT                   linker script for every executable linking the kernel
#
##########################################################################

set(OSAL_FREERTOS_CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(OSAL_FREERTOS_GENERIC_DIR ${OSAL_FREERTOS_CONFIG_DIR} DIRECTORY)

function(osal_freertos_platform)
    set(required_keys PORT CONSOLE TICK_RATE_HZ MINIMAL_STACK_SIZE TOTAL_HEAP_SIZE)
    set(optional_value_keys CPU_CLOCK_HZ KERNEL_INTERRUPT_PRIORITY MAX_SYSCALL_INTERRUPT_PRIORITY)
    cmake_parse_arguments(PLATFORM ""
        "${required_keys};${optional_value_keys};OPTIMISED_TASK_SELECTION;PORT_CLEAN_UP_TCB;LINKER_SCRIPT"
        "HEADERS;CONSOLE_DEFINES;SOURCES"
        ${ARGN})

    if (PLATFORM_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "osal_freertos_platform: unknown arguments: ${PLATFORM_UNPARSED_ARGUMENTS}")
    endif ()
    foreach (key IN LISTS required_keys)
        if ("${PLATFORM_${key}}" STREQUAL "")
            message(FATAL_ERROR "osal_freertos_platform: ${key} is required - see ${OSAL_FREERTOS_CONFIG_DIR}/osal_freertos_platform.cmake")
        endif ()
    endforeach ()
    if ("${PLATFORM_OPTIMISED_TASK_SELECTION}" STREQUAL "")
        set(PLATFORM_OPTIMISED_TASK_SELECTION 0)
    endif ()

    set(console_backend ${OSAL_FREERTOS_GENERIC_DIR}/console/${PLATFORM_CONSOLE}.h)
    if (NOT EXISTS ${console_backend})
        message(FATAL_ERROR "osal_freertos_platform: no console backend '${PLATFORM_CONSOLE}' (expected ${console_backend})")
    endif ()

    # Generated header: optional macros are emitted only when set, so
    # FreeRTOSConfig.h tests them with defined()
    set(PLATFORM_INCLUDES "")
    foreach (header IN LISTS PLATFORM_HEADERS)
        string(APPEND PLATFORM_INCLUDES "#include <${header}>\n")
    endforeach ()
    set(PLATFORM_OPTIONAL_DEFINES "")
    foreach (key IN LISTS optional_value_keys)
        if (NOT "${PLATFORM_${key}}" STREQUAL "")
            string(APPEND PLATFORM_OPTIONAL_DEFINES "#define OSAL_FREERTOS_PLATFORM_${key} (${PLATFORM_${key}})\n")
        endif ()
    endforeach ()
    if (NOT "${PLATFORM_PORT_CLEAN_UP_TCB}" STREQUAL "")
        string(APPEND PLATFORM_OPTIONAL_DEFINES
            "#define OSAL_FREERTOS_PLATFORM_PORT_CLEAN_UP_TCB(pxTCB) ${PLATFORM_PORT_CLEAN_UP_TCB}\n")
    endif ()
    configure_file(${OSAL_FREERTOS_CONFIG_DIR}/FreeRTOSConfigPlatform.h.in
                   ${CMAKE_CURRENT_BINARY_DIR}/config/FreeRTOSConfigPlatform.h @ONLY)

    # The BSP is authoritative for the port, so this overrides any cached
    # value; it is read by the kernel's add_subdirectory, which comes later
    set(FREERTOS_PORT ${PLATFORM_PORT} CACHE STRING "FreeRTOS port name, set by the BSP" FORCE)

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/config
        ${OSAL_FREERTOS_CONFIG_DIR})

    # The kernel links freertos_config, and osal_bsp links the kernel (see the
    # top-level CMakeLists.txt), so this reaches every executable using the BSP
    if (PLATFORM_LINKER_SCRIPT)
        get_filename_component(linker_script ${PLATFORM_LINKER_SCRIPT} ABSOLUTE
                               BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
        if (NOT EXISTS ${linker_script})
            message(FATAL_ERROR "osal_freertos_platform: LINKER_SCRIPT ${linker_script} does not exist")
        endif ()
        target_link_options(freertos_config INTERFACE -T${linker_script})
    endif ()

    set(impl osal_${OSAL_SYSTEM_BSPTYPE}_impl)
    add_library(${impl} OBJECT
        ${OSAL_FREERTOS_GENERIC_DIR}/src/bsp_start.c
        ${OSAL_FREERTOS_GENERIC_DIR}/src/bsp_console.c
        ${PLATFORM_SOURCES}
    )
    # Lets bsp_console.c resolve OSAL_FREERTOS_PLATFORM_CONSOLE_BACKEND ("console/<CONSOLE>.h")
    target_include_directories(${impl} PRIVATE ${OSAL_FREERTOS_GENERIC_DIR})
    target_compile_definitions(${impl} PRIVATE ${PLATFORM_CONSOLE_DEFINES})
    target_link_libraries(${impl} PUBLIC freertos_kernel)
endfunction()
