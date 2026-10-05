##########################################################################
#
# osal_freertos_platform(): the one declaration a FreeRTOS BSP makes, for the
# PORT named in the vendor's -C config file (not an argument). It generates
# FreeRTOSConfigPlatform.h and creates the freertos_config target the kernel
# reads and the osal_<bsp>_impl target OSAL builds the BSP from.
#
#   CONSOLE                     console backend console/<CONSOLE>.h; without it the
#                               vendor's sources define OS_FreeRTOS_ConsoleWrite
#                               and OS_FreeRTOS_PlatformHalt
#   CORE_CLOCK                  the port needs OSAL_FREERTOS_CPU_CLOCK_HZ
#   INTERRUPT_PRIORITIES        the port needs the two priority keys
#   OPTIMISED_TASK_SELECTION    configUSE_PORT_OPTIMISED_TASK_SELECTION (default 0)
#   PORT_CLEAN_UP_TCB           cleanup expression of pxTCB, only for a port whose
#                               portmacro.h already defines portCLEAN_UP_TCB
#   PORT_DEFINES                NAME=VALUE pairs the port's portmacro.h requires
#   HEADERS                     system headers the port's values need
#   CONSOLE_DEFINES             compile definitions the console backend needs
#   DEFAULT_MINIMAL_STACK_SIZE  stack size when the config file sets none (GCC_POSIX)
#
# The vendor supplies the port and the chip's values in a CMake file applied with
# cmake -C <file>. Each set() needs CACHE STRING "" FORCE: the values only reach
# the build through the cache, and without FORCE an entry already there wins.
# Sizes are bytes, optionally with a K or M suffix. Use absolute paths. A key the
# port does not use, a missing key and an unknown OSAL_FREERTOS_ name are
# rejected. Keys from an earlier configure persist in the cache, so use a fresh
# build directory when switching configs.
#
#   [port]
#   set(PORT GCC_ARM_CM4F CACHE STRING "" FORCE)
#   [kernel]  tick in Hz, idle-task stack and heap in bytes
#   set(OSAL_FREERTOS_TICK_RATE_HZ 1000 CACHE STRING "" FORCE)
#   set(OSAL_FREERTOS_MINIMAL_STACK_SIZE 2K CACHE STRING "" FORCE)
#   set(OSAL_FREERTOS_TOTAL_HEAP_SIZE 64K CACHE STRING "" FORCE)
#   [chip]  core clock in Hz; priorities as C expressions in the implemented bits
#   set(OSAL_FREERTOS_CPU_CLOCK_HZ 48000000 CACHE STRING "" FORCE)
#   set(OSAL_FREERTOS_KERNEL_INTERRUPT_PRIORITY "(15 << 4)" CACHE STRING "" FORCE)
#   set(OSAL_FREERTOS_MAX_SYSCALL_INTERRUPT_PRIORITY "(5 << 4)" CACHE STRING "" FORCE)
#   [build inputs]  startup code, vector table and console functions; linker script
#   set(OSAL_FREERTOS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/startup.c;${CMAKE_CURRENT_LIST_DIR}/vectors.c" CACHE STRING "" FORCE)
#   set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
#
##########################################################################

set(OSAL_FREERTOS_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(OSAL_FREERTOS_GENERIC_DIR ${OSAL_FREERTOS_PLATFORM_DIR} DIRECTORY)

# Section of the config file a key belongs to, for messages
function(osal_freertos_config_section key out)
    if (key MATCHES "^(TICK_RATE_HZ|MINIMAL_STACK_SIZE|TOTAL_HEAP_SIZE)$")
        set(${out} "kernel" PARENT_SCOPE)
    elseif (key MATCHES "^(CPU_CLOCK_HZ|KERNEL_INTERRUPT_PRIORITY|MAX_SYSCALL_INTERRUPT_PRIORITY)$")
        set(${out} "chip" PARENT_SCOPE)
    else ()
        set(${out} "build inputs" PARENT_SCOPE)
    endif ()
endfunction()

# Reads a size - bytes, or a number with a K or M suffix (1024-based) - as bytes.
# Other text is kept as a C expression (PTHREAD_STACK_MIN); any other suffix is rejected.
function(osal_freertos_size_in_bytes name value out)
    if ("${value}" MATCHES "^([0-9]+)([KkMm])(iB)?$")
        set(number ${CMAKE_MATCH_1})
        string(TOUPPER "${CMAKE_MATCH_2}" unit)
        if (unit STREQUAL "K")
            math(EXPR number "${number} * 1024")
        else ()
            math(EXPR number "${number} * 1024 * 1024")
        endif ()
        set(value ${number})
    elseif ("${value}" MATCHES "^[0-9]+[A-Za-z]+$")
        message(FATAL_ERROR "${name}='${value}' is not a size: write bytes, or a number with a K or M suffix (1024-based), "
                            "e.g. 65536, 64K, 16M")
    endif ()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(osal_freertos_platform)
    cmake_parse_arguments(PLATFORM "CORE_CLOCK;INTERRUPT_PRIORITIES"
        "CONSOLE;OPTIMISED_TASK_SELECTION;PORT_CLEAN_UP_TCB;DEFAULT_MINIMAL_STACK_SIZE"
        "HEADERS;CONSOLE_DEFINES;PORT_DEFINES"
        ${ARGN})

    if (PLATFORM_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "osal_freertos_platform: unknown arguments: ${PLATFORM_UNPARSED_ARGUMENTS}")
    endif ()
    if ("${PORT}" STREQUAL "")
        message(FATAL_ERROR "osal_freertos_platform: PORT is not set - set it in the -C config file")
    endif ()
    if ("${PLATFORM_OPTIMISED_TASK_SELECTION}" STREQUAL "")
        set(PLATFORM_OPTIMISED_TASK_SELECTION 0)
    endif ()

    set(PLATFORM_CONSOLE_BACKEND_DEFINE "")
    if (NOT "${PLATFORM_CONSOLE}" STREQUAL "")
        set(console_backend ${OSAL_FREERTOS_GENERIC_DIR}/console/${PLATFORM_CONSOLE}.h)
        if (NOT EXISTS ${console_backend})
            message(FATAL_ERROR "osal_freertos_platform: no console backend '${PLATFORM_CONSOLE}' (expected ${console_backend})")
        endif ()
        set(PLATFORM_CONSOLE_BACKEND_DEFINE "#define OSAL_FREERTOS_PLATFORM_CONSOLE_BACKEND \"console/${PLATFORM_CONSOLE}.h\"\n")
    endif ()

    # The vendor's values arrive in the cache; any other OSAL_FREERTOS_ name there is a misspelled key
    set(known_names OSAL_FREERTOS_STORAGE_TARGET)
    foreach (key IN ITEMS TICK_RATE_HZ MINIMAL_STACK_SIZE TOTAL_HEAP_SIZE
                          CPU_CLOCK_HZ KERNEL_INTERRUPT_PRIORITY MAX_SYSCALL_INTERRUPT_PRIORITY
                          SOURCES LINKER_SCRIPT)
        list(APPEND known_names OSAL_FREERTOS_${key})
    endforeach ()
    get_cmake_property(cache_variables CACHE_VARIABLES)
    foreach (variable IN LISTS cache_variables)
        if (variable MATCHES "^OSAL_FREERTOS_" AND NOT variable IN_LIST known_names)
            message(FATAL_ERROR "${variable} is not a known key of the -C config file - see "
                                "${OSAL_FREERTOS_PLATFORM_DIR}/osal_freertos_platform.cmake")
        endif ()
    endforeach ()

    # The keys this port uses are required (bar a declared default); any other key is an error
    set(vendor_keys TICK_RATE_HZ MINIMAL_STACK_SIZE TOTAL_HEAP_SIZE)
    set(unused_keys "")
    if (PLATFORM_CORE_CLOCK)
        list(APPEND vendor_keys CPU_CLOCK_HZ)
    else ()
        list(APPEND unused_keys CPU_CLOCK_HZ)
    endif ()
    if (PLATFORM_INTERRUPT_PRIORITIES)
        list(APPEND vendor_keys KERNEL_INTERRUPT_PRIORITY MAX_SYSCALL_INTERRUPT_PRIORITY)
    else ()
        list(APPEND unused_keys KERNEL_INTERRUPT_PRIORITY MAX_SYSCALL_INTERRUPT_PRIORITY)
    endif ()
    list(APPEND vendor_keys SOURCES LINKER_SCRIPT)
    foreach (key IN LISTS unused_keys)
        if (NOT "${OSAL_FREERTOS_${key}}" STREQUAL "")
            osal_freertos_config_section(${key} section)
            message(FATAL_ERROR "[${section}] OSAL_FREERTOS_${key} does not apply to PORT=${PORT}")
        endif ()
    endforeach ()
    set(vendor_definitions "")
    foreach (key IN LISTS vendor_keys)
        set(value "${OSAL_FREERTOS_${key}}")
        if ("${value}" STREQUAL "")
            set(value "${PLATFORM_DEFAULT_${key}}")
        endif ()
        if ("${value}" STREQUAL "")
            osal_freertos_config_section(${key} section)
            message(FATAL_ERROR "[${section}] OSAL_FREERTOS_${key} is required for PORT=${PORT}: set it in the -C config file with CACHE STRING \"\" FORCE - see "
                                "${OSAL_FREERTOS_PLATFORM_DIR}/osal_freertos_platform.cmake")
        endif ()
        if (key MATCHES "^(MINIMAL_STACK_SIZE|TOTAL_HEAP_SIZE)$")
            osal_freertos_size_in_bytes("OSAL_FREERTOS_${key}" "${value}" value)
        endif ()
        if (NOT key MATCHES "^(SOURCES|LINKER_SCRIPT)$")
            if ("${value}" MATCHES "[\r\n;]")
                message(FATAL_ERROR "OSAL_FREERTOS_${key} must be a single C expression without ';' or newlines, "
                                    "got '${value}'")
            endif ()
            list(APPEND vendor_definitions "OSAL_FREERTOS_${key}=${value}")
        endif ()
    endforeach ()
    # Generated header: optional macros are emitted only when set, so
    # FreeRTOSConfig.h tests them with defined()
    set(PLATFORM_INCLUDES "")
    foreach (header IN LISTS PLATFORM_HEADERS)
        string(APPEND PLATFORM_INCLUDES "#include <${header}>\n")
    endforeach ()
    set(PLATFORM_OPTIONAL_DEFINES "")
    if (PLATFORM_CORE_CLOCK)
        string(APPEND PLATFORM_OPTIONAL_DEFINES "#define OSAL_FREERTOS_PLATFORM_NEEDS_CPU_CLOCK 1\n")
    endif ()
    if (PLATFORM_INTERRUPT_PRIORITIES)
        string(APPEND PLATFORM_OPTIONAL_DEFINES "#define OSAL_FREERTOS_PLATFORM_HAS_INTERRUPT_PRIORITIES 1\n")
    endif ()
    foreach (define IN LISTS PLATFORM_PORT_DEFINES)
        string(FIND "${define}" "=" separator)
        if (separator LESS 1)
            message(FATAL_ERROR "osal_freertos_platform: PORT_DEFINES entry '${define}' is not NAME=VALUE")
        endif ()
        string(SUBSTRING "${define}" 0 ${separator} define_name)
        math(EXPR value_start "${separator} + 1")
        string(SUBSTRING "${define}" ${value_start} -1 define_value)
        string(APPEND PLATFORM_OPTIONAL_DEFINES "#define ${define_name} ${define_value}\n")
    endforeach ()
    if (NOT "${PLATFORM_PORT_CLEAN_UP_TCB}" STREQUAL "")
        string(APPEND PLATFORM_OPTIONAL_DEFINES
            "#define OSAL_FREERTOS_PLATFORM_PORT_CLEAN_UP_TCB(pxTCB) ${PLATFORM_PORT_CLEAN_UP_TCB}\n")
    endif ()
    configure_file(${OSAL_FREERTOS_PLATFORM_DIR}/FreeRTOSConfigPlatform.h.in
                   ${CMAKE_CURRENT_BINARY_DIR}/config/FreeRTOSConfigPlatform.h @ONLY)

    # The BSP is authoritative for the port, so this overrides any cached
    # value; it is read by the kernel's add_subdirectory, which comes later
    set(FREERTOS_PORT ${PORT} CACHE STRING "FreeRTOS port name, set by the BSP" FORCE)

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/config
        ${OSAL_FREERTOS_PLATFORM_DIR})
    target_compile_definitions(freertos_config INTERFACE ${vendor_definitions})

    # The kernel links freertos_config, and osal_bsp links the kernel (see the
    # top-level CMakeLists.txt), so this reaches every executable using the BSP
    if (NOT "${OSAL_FREERTOS_LINKER_SCRIPT}" STREQUAL "")
        get_filename_component(linker_script "${OSAL_FREERTOS_LINKER_SCRIPT}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if (NOT EXISTS "${linker_script}")
            message(FATAL_ERROR "[build inputs] OSAL_FREERTOS_LINKER_SCRIPT ${linker_script} does not exist")
        endif ()
        target_link_options(freertos_config INTERFACE -T${linker_script})
    endif ()

    set(impl osal_${OSAL_SYSTEM_BSPTYPE}_impl)
    add_library(${impl} OBJECT
        ${OSAL_FREERTOS_GENERIC_DIR}/src/bsp_start.c
        ${OSAL_FREERTOS_GENERIC_DIR}/src/bsp_console.c
        ${OSAL_FREERTOS_SOURCES}
    )
    # Lets bsp_console.c resolve OSAL_FREERTOS_PLATFORM_CONSOLE_BACKEND ("console/<CONSOLE>.h")
    target_include_directories(${impl} PRIVATE ${OSAL_FREERTOS_GENERIC_DIR})
    target_compile_definitions(${impl} PRIVATE ${PLATFORM_CONSOLE_DEFINES})
    target_link_libraries(${impl} PUBLIC freertos_kernel)
endfunction()
