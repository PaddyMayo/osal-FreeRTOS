##########################################################################
#
# Simulation tests: build the product for one platform, link a test
# firmware against it, and run that firmware under QEMU, driven through gdb.
#
# This is a CMake project of its own, so nothing in the product build knows
# about QEMU: it takes the product tree as a subproject and the platform from
# the same -C config file the product uses, e.g.
#
#   cmake -S freertos-sim-tests -B build/sim-cortex-m3 -C platforms/cortex-m3/config.cmake
#   cmake --build build/sim-cortex-m3 -j
#   ctest --test-dir build/sim-cortex-m3 --output-on-failure
#
# The firmware is run as it is built: the harness (gdb/run_firmware_test.py)
# only breaks on functions the BSP already has.
#
##########################################################################

cmake_minimum_required(VERSION 3.16)
project(osal_freertos_sim_tests C)

enable_testing()

# The product's own tests are built for the host, not for this platform
set(ENABLE_UNIT_TESTS FALSE CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. product)

# ---- How each platform is run ------------------------------------------------
# A Cortex-M port is a bare-metal image: QEMU emulates a board and gdb drives
# it (QEMU_MACHINE). The POSIX port is a Linux program: it is run directly, or
# under QEMU's user-mode emulation when it is built for another architecture.
if ("${PORT}" STREQUAL "GCC_POSIX")
    set(QEMU_KIND process)
    if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        set(QEMU_USER qemu-aarch64)
        set(QEMU_USER_SYSROOT /usr/aarch64-linux-gnu)
        set(TEST_SUFFIX -aarch64)
    else ()
        message(FATAL_ERROR "No QEMU user-mode emulator is known for ${CMAKE_SYSTEM_PROCESSOR}, add it to ${CMAKE_CURRENT_LIST_FILE}")
    endif ()
else ()
    set(QEMU_KIND board)
    set(QEMU_SYSTEM qemu-system-arm)
    if ("${PORT}" STREQUAL "GCC_ARM_CM0")
        set(QEMU_MACHINE microbit)
    elseif ("${PORT}" STREQUAL "GCC_ARM_CM3")
        set(QEMU_MACHINE mps2-an385)
    elseif ("${PORT}" STREQUAL "GCC_ARM_CM4F")
        set(QEMU_MACHINE mps2-an386)
    elseif ("${PORT}" STREQUAL "GCC_ARM_CM7")
        set(QEMU_MACHINE mps2-an500)
    elseif ("${PORT}" STREQUAL "GCC_ARM_CM33_NTZ_NONSECURE")
        set(QEMU_MACHINE mps2-an505)
    else ()
        message(FATAL_ERROR "No QEMU machine is known for PORT=${PORT}, add it to ${CMAKE_CURRENT_LIST_FILE}")
    endif ()
endif ()

# ---- The firmware under test -------------------------------------------------
# smoke runs on the kernel alone. The others call the OSAL API, which does not
# link on the port yet (only common, queue and tasks are implemented), so they
# take src/osal_stubs.c for the rest.
set(SIM_FIRMWARE smoke)
set(SIM_FIRMWARE_OSAL task_reuse)
if ("${PORT}" STREQUAL "GCC_ARM_CM0")
    # The micro:bit has 16 KB of RAM, OSAL's object tables alone overflow it
    set(SIM_FIRMWARE_OSAL)
endif ()

string(TOLOWER "${PORT}" PORT_LOWER)

if (QEMU_KIND STREQUAL "board")
    find_program(QEMU_GDB NAMES gdb-multiarch arm-none-eabi-gdb)
    find_program(QEMU_SYSTEM_PATH NAMES ${QEMU_SYSTEM})
    if (NOT QEMU_GDB OR NOT QEMU_SYSTEM_PATH)
        message(WARNING "gdb-multiarch and ${QEMU_SYSTEM} are needed to run the tests, so none are registered")
    endif ()
else ()
    find_program(QEMU_USER_PATH NAMES ${QEMU_USER})
    if (NOT QEMU_USER_PATH)
        message(WARNING "${QEMU_USER} is needed to run the tests, so none are registered")
    endif ()
endif ()

foreach (FIRMWARE IN LISTS SIM_FIRMWARE SIM_FIRMWARE_OSAL)
    add_executable(sim_${FIRMWARE} src/${FIRMWARE}.c)
    target_link_libraries(sim_${FIRMWARE} PRIVATE osal_bsp osal)
    if (FIRMWARE IN_LIST SIM_FIRMWARE_OSAL)
        target_sources(sim_${FIRMWARE} PRIVATE src/osal_stubs.c)
        target_include_directories(sim_${FIRMWARE} PRIVATE ${OSAL_SOURCE_DIR}/src/os/shared/inc)
    endif ()

    set(TEST_NAME sim-${PORT_LOWER}${TEST_SUFFIX}-${FIRMWARE})

    if (QEMU_KIND STREQUAL "board")
        if (QEMU_GDB AND QEMU_SYSTEM_PATH)
            add_test(NAME ${TEST_NAME}
                COMMAND ${QEMU_GDB} -q -batch -x ${CMAKE_CURRENT_SOURCE_DIR}/gdb/run_firmware_test.py --args $<TARGET_FILE:sim_${FIRMWARE}>
            )
            set_tests_properties(${TEST_NAME} PROPERTIES
                ENVIRONMENT "QEMU_SYSTEM=${QEMU_SYSTEM_PATH};QEMU_MACHINE=${QEMU_MACHINE}"
                TIMEOUT 120
            )
        endif ()
    elseif (QEMU_USER_PATH)
        # The BSP returns the application's status from main, so the exit status is the result
        add_test(NAME ${TEST_NAME} COMMAND ${QEMU_USER_PATH} -L ${QEMU_USER_SYSROOT} $<TARGET_FILE:sim_${FIRMWARE}>)
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 120)
    endif ()
endforeach ()

# ---- The halt ------------------------------------------------------------------
# Each way the BSP halts, and a firmware that spins as the control (EXPECT busy)
# the CPU measurement is checked against. A Cortex-M halts when the application
# returns; the POSIX port ends the scheduler and exits instead, so it has no
# "exit" case. It has no "overflow" case either: a POSIX task runs on a real
# pthread stack, not the stack FreeRTOS checks, so it cannot be made to overflow.
# The boards and the process are watched differently, see the harnesses in gdb/.
set(HALT_CASES assert overflow exit busy)
if (QEMU_KIND STREQUAL "process")
    set(HALT_CASES assert busy)
elseif ("${PORT}" STREQUAL "GCC_ARM_CM33_NTZ_NONSECURE")
    # The Cortex-M33 checks the stack limit in hardware (PSPLIM), so an overflow
    # is a HardFault before FreeRTOS's own check runs, and that spins by design
    set(HALT_CASES assert exit busy)
endif ()

find_package(Python3 COMPONENTS Interpreter)

foreach (CASE IN LISTS HALT_CASES)
    string(TOUPPER "${CASE}" CASE_UPPER)
    add_executable(sim_halt_${CASE} src/halt.c)
    target_compile_definitions(sim_halt_${CASE} PRIVATE HALT_CASE_${CASE_UPPER})
    target_link_libraries(sim_halt_${CASE} PRIVATE osal_bsp osal)

    set(TEST_NAME sim-${PORT_LOWER}${TEST_SUFFIX}-halt-${CASE})
    if (CASE STREQUAL "busy")
        set(EXPECT busy)
    else ()
        set(EXPECT idle)
    endif ()

    if (QEMU_KIND STREQUAL "board")
        if (QEMU_GDB AND QEMU_SYSTEM_PATH)
            add_test(NAME ${TEST_NAME}
                COMMAND ${QEMU_GDB} -q -batch -x ${CMAKE_CURRENT_SOURCE_DIR}/gdb/run_halt_test.py --args $<TARGET_FILE:sim_halt_${CASE}>
            )
            set_tests_properties(${TEST_NAME} PROPERTIES
                ENVIRONMENT "QEMU_SYSTEM=${QEMU_SYSTEM_PATH};QEMU_MACHINE=${QEMU_MACHINE};EXPECT=${EXPECT}"
                TIMEOUT 120
            )
        endif ()
    elseif (QEMU_USER_PATH AND Python3_FOUND)
        add_test(NAME ${TEST_NAME}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gdb/run_halt_test_posix.py
                    ${EXPECT} ${QEMU_USER_PATH} ${QEMU_USER_SYSROOT} $<TARGET_FILE:sim_halt_${CASE}>
        )
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 120)
    endif ()
endforeach ()
