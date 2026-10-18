# Vendor config for the GCC_POSIX build for aarch64 Linux that CI runs:
#   cmake -S . -B build/posix-aarch64 -C platforms/posix-aarch64/config.cmake -DENABLE_UNIT_TESTS=OFF
#
# The same host simulator as platforms/posix, cross compiled for another
# architecture, so the port is built and run on a CPU other than the host's.
set(PORT GCC_POSIX CACHE STRING "" FORCE)

set(CMAKE_SYSTEM_NAME      Linux                CACHE STRING "" FORCE)
set(CMAKE_SYSTEM_PROCESSOR aarch64              CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER       aarch64-linux-gnu-gcc CACHE STRING "" FORCE)

# The minimal stack size is left to GCC_POSIX's default (PTHREAD_STACK_MIN)
set(OSAL_FREERTOS_TICK_RATE_HZ    100 CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE 16777216 CACHE STRING "" FORCE)

# Every port names startup code and a linker script. A Linux program has no
# vector table, so the startup code is a placeholder, see startup.c, and the
# linker script adds nothing to the toolchain's own, see link.ld.
set(OSAL_FREERTOS_SOURCES       "${CMAKE_CURRENT_LIST_DIR}/startup.c;${CMAKE_CURRENT_LIST_DIR}/../common/storage-ram/bsp_storage.c;${CMAKE_CURRENT_LIST_DIR}/../common/io-null/bsp_io.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
