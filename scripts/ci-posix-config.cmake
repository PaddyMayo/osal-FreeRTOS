# Vendor config for the GCC_POSIX build that CI and scripts/coverage.sh run:
#   cmake -S . -B build -C scripts/ci-posix-config.cmake
set(PORT GCC_POSIX CACHE STRING "" FORCE)

# The minimal stack size is left to GCC_POSIX's default (PTHREAD_STACK_MIN)
set(OSAL_FREERTOS_TICK_RATE_HZ    100 CACHE STRING "" FORCE)
set(OSAL_FREERTOS_TOTAL_HEAP_SIZE 16M CACHE STRING "" FORCE)

# Every port names startup code and a linker script, which a host build lacks.
# These placeholders serve what CI builds (osal, the coverage tests), not an executable.
set(_ci_dir "${CMAKE_BINARY_DIR}/osal_freertos_ci")
file(WRITE "${_ci_dir}/startup.c" "typedef int osal_freertos_ci_placeholder;\n")
file(WRITE "${_ci_dir}/link.ld" "/* placeholder */\n")
set(OSAL_FREERTOS_SOURCES       "${_ci_dir}/startup.c" CACHE STRING "" FORCE)
set(OSAL_FREERTOS_LINKER_SCRIPT "${_ci_dir}/link.ld" CACHE STRING "" FORCE)
