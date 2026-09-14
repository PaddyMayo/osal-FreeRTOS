##########################################################################
#
# Build options for "freertos" OS layer
#
##########################################################################

# osal only picks up osal_freertos_impl's compiled objects via
# $<TARGET_OBJECTS:...>, which does not inherit that target's own
# target_link_libraries(freertos_kernel) - so it must be added here explicitly,
# otherwise anything linking osal fails with undefined references to
# FreeRTOS task/queue/scheduler symbols.
target_link_libraries(osal PUBLIC freertos_kernel)
