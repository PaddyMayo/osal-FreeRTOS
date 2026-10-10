##########################################################################
#
# Build options for "generic-freertos" BSP
#
##########################################################################

# Configure the ut_coverage_compile and ut_coverage_link for enabling coverage
# testing on this platform.
# Note: --coverage is just a shortcut for "-ftest-coverage" and "-fprofile-arcs"
# This does not work well when cross compiling since paths to the _compile_ dir
# are baked into the executables, so they will not be there when copied to the target
if (NOT CMAKE_CROSSCOMPILING AND ENABLE_UNIT_TESTS)
  # Support for other compilers/coverage tools could be added here.
  # for now only the GNU "gcov" will be enabled
  if (CMAKE_C_COMPILER_ID STREQUAL GNU)
    target_compile_options(ut_coverage_compile INTERFACE --coverage)
    target_link_options(ut_coverage_link INTERFACE --coverage)
  endif()
endif()
