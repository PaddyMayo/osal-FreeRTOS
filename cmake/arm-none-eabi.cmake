# Toolchain file for the Cortex-M platforms:
#   cmake -S . -B build -C platforms/<name>/config.cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
# The CPU flags (-mcpu, -mthumb, -mfloat-abi) come from the platform's config.cmake.
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_AR           arm-none-eabi-ar)
set(CMAKE_RANLIB       arm-none-eabi-ranlib)

# No startup code or linker script exists at compiler-check time
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
