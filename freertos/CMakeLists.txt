add_library(osal_freertos_impl OBJECT
    src/os-impl-tasks.c
    src/os-impl-queue.c
)
target_include_directories(osal_freertos_impl PUBLIC
    inc
    ${OSAL_SOURCE_DIR}/src/os/inc
)
target_link_libraries(osal_freertos_impl PUBLIC freertos_kernel)