# Vendor config for the GCC_POSIX host build that CI and the freertos-coverage pre-commit hooks run:
#   cmake -S . -B build/posix -C platforms/posix/config.cmake
set(PORT GCC_POSIX CACHE STRING "" FORCE)

# The minimal stack size is left to GCC_POSIX's default (PTHREAD_STACK_MIN)
set(OSAL_FREERTOS_TICK_RATE_HZ    100 CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE 16777216 CACHE STRING "" FORCE)

# Every port names startup code and a linker script. A host build has no vector
# table, so both are placeholders, see startup.c and link.ld.
set(OSAL_FREERTOS_SOURCES       "${CMAKE_CURRENT_LIST_DIR}/startup.c;${CMAKE_CURRENT_LIST_DIR}/../common/storage-ram/bsp_storage.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/link.ld" CACHE STRING "" FORCE)
