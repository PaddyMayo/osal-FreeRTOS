##########################################################################
#
# Build options for "generic-freertos-posix" BSP
#
##########################################################################

# osal_bsp only picks up osal_generic-freertos-posix_impl's compiled objects via
# $<TARGET_OBJECTS:...>, which does not inherit that target's own
# target_link_libraries(freertos_kernel) - so it must be added here explicitly,
# otherwise anything linking osal_bsp fails with undefined references to
# FreeRTOS task/queue/scheduler symbols.
target_link_libraries(osal_bsp PUBLIC freertos_kernel)
