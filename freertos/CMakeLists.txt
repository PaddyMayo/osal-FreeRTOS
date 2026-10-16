file(GLOB OSAL_FREERTOS_IMPL_SOURCES CONFIGURE_DEPENDS src/os-impl-*.c)

# littlefs as seen by the BSP: the headers and the config macro, with no
# sources. The storage source links this for lfs.h
add_library(osal_littlefs INTERFACE)
target_include_directories(osal_littlefs SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party/littlefs)
# Each open file takes its cache from a static buffer, so littlefs must not malloc
target_compile_definitions(osal_littlefs INTERFACE LFS_NO_MALLOC)

# The library itself is compiled with the port, so there is one copy of it
add_library(osal_freertos_impl OBJECT
    ${OSAL_FREERTOS_IMPL_SOURCES}
    ${CMAKE_SOURCE_DIR}/third_party/littlefs/lfs.c
    ${CMAKE_SOURCE_DIR}/third_party/littlefs/lfs_util.c
)
target_include_directories(osal_freertos_impl PUBLIC
    inc
    ${OSAL_SOURCE_DIR}/src/os/inc
)
target_link_libraries(osal_freertos_impl PUBLIC freertos_kernel osal_littlefs)
