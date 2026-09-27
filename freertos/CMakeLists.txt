file(GLOB OSAL_FREERTOS_IMPL_SOURCES CONFIGURE_DEPENDS src/os-impl-*.c)

add_library(osal_freertos_impl OBJECT
    ${OSAL_FREERTOS_IMPL_SOURCES}
)
target_include_directories(osal_freertos_impl PUBLIC
    inc
    ${OSAL_SOURCE_DIR}/src/os/inc
)
target_link_libraries(osal_freertos_impl PUBLIC freertos_kernel)