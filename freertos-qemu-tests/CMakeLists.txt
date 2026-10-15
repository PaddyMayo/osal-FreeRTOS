##########################################################################
#
# QEMU firmware tests: build the product for one platform, link a test
# firmware against it, and run that firmware under QEMU, driven through gdb.
#
# This is a CMake project of its own, so nothing in the product build knows
# about QEMU: it takes the product tree as a subproject and the platform from
# the same -C config file the product uses, e.g.
#
#   cmake -S freertos-qemu-tests -B build/qemu-cortex-m3 -C platforms/cortex-m3/config.cmake
#   cmake --build build/qemu-cortex-m3 -j
#   ctest --test-dir build/qemu-cortex-m3 --output-on-failure
#
# The firmware is run as it is built: the harness (gdb/run_firmware_test.py)
# only breaks on functions the BSP already has.
#
##########################################################################

cmake_minimum_required(VERSION 3.16)
project(osal_freertos_qemu_tests C)

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
add_executable(qemu_smoke src/smoke.c)
target_link_libraries(qemu_smoke PRIVATE osal_bsp osal)

# ---- Tests ---------------------------------------------------------------------
string(TOLOWER "${PORT}" PORT_LOWER)
set(TEST_NAME qemu-${PORT_LOWER}${TEST_SUFFIX}-smoke)

if (QEMU_KIND STREQUAL "board")
    find_program(QEMU_GDB NAMES gdb-multiarch arm-none-eabi-gdb)
    find_program(QEMU_SYSTEM_PATH NAMES ${QEMU_SYSTEM})
    if (NOT QEMU_GDB OR NOT QEMU_SYSTEM_PATH)
        message(WARNING "gdb-multiarch and ${QEMU_SYSTEM} are needed to run the tests, so none are registered")
        return()
    endif ()

    add_test(NAME ${TEST_NAME}
        COMMAND ${QEMU_GDB} -q -batch -x ${CMAKE_CURRENT_SOURCE_DIR}/gdb/run_firmware_test.py --args $<TARGET_FILE:qemu_smoke>
    )
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT "QEMU_SYSTEM=${QEMU_SYSTEM_PATH};QEMU_MACHINE=${QEMU_MACHINE}"
        TIMEOUT 120
    )
else ()
    # The BSP returns the application's status from main, so the exit status is the result
    find_program(QEMU_USER_PATH NAMES ${QEMU_USER})
    if (NOT QEMU_USER_PATH)
        message(WARNING "${QEMU_USER} is needed to run the tests, so none are registered")
        return()
    endif ()

    add_test(NAME ${TEST_NAME} COMMAND ${QEMU_USER_PATH} -L ${QEMU_USER_SYSROOT} $<TARGET_FILE:qemu_smoke>)
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 120)
endif ()
