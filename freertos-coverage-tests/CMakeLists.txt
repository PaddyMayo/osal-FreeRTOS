##########################################################################
#
# White-box coverage tests for the FreeRTOS OSAL port.
#
# Kept entirely outside third_party/osal: OSAL's own unit-test-coverage/
# tree requires a new port's coverage directory to physically exist inside
# the vendored source (no OSAL_EXT_SOURCE_DIR-style external hook exists for
# it). This instead compiles ut_assert's own (OS-agnostic) source files
# directly - reusing them without needing the packaged `ut_assert` CMake
# target, which PUBLICLY links `osal_bsp` (and therefore, in this project,
# the real freertos_kernel and its scheduler-driving main()) via
# third_party/osal/ut_assert/CMakeLists.txt. Nothing inside third_party/osal
# is modified.
#
# NOTE: OSAL_SOURCE_DIR is a variable local to third_party/osal's own
# add_subdirectory() scope (set implicitly by its `project(OSAL ...)` call),
# not visible here as a CMake variable - real OSAL/ut_assert paths below are
# written relative to CMAKE_SOURCE_DIR instead.
#
##########################################################################

set(OSAL_REAL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/osal)
set(UT_ASSERT_REAL_SOURCE_DIR ${OSAL_REAL_SOURCE_DIR}/ut_assert)

set(FREERTOS_COVERAGE_INCLUDES
    fake-inc
    ${OSAL_REAL_SOURCE_DIR}/src/os/inc
    ${OSAL_REAL_SOURCE_DIR}/src/os/shared/inc
    ${CMAKE_BINARY_DIR}/osal_build/inc
    ${UT_ASSERT_REAL_SOURCE_DIR}/inc
)

# The file under test, compiled with gcov instrumentation and pointed at our
# fake FreeRTOS headers (fake-inc) instead of the real kernel. Applying
# --coverage directly (rather than through OSAL's ut_coverage_compile/
# ut_coverage_link interface targets) avoids those targets' own transitive
# link to ut_assert -> osal_bsp.
add_library(utobj_freertos_tasks OBJECT
    ${CMAKE_SOURCE_DIR}/freertos/src/os-impl-tasks.c
)
target_include_directories(utobj_freertos_tasks BEFORE PRIVATE fake-inc)
target_include_directories(utobj_freertos_tasks PRIVATE
    ${OSAL_REAL_SOURCE_DIR}/src/os/inc
    ${OSAL_REAL_SOURCE_DIR}/src/os/shared/inc
    ${CMAKE_BINARY_DIR}/osal_build/inc
)
target_compile_options(utobj_freertos_tasks PRIVATE --coverage)

# The stubs, test cases, and ut_assert's own (BSP-independent) sources are
# compiled normally - no coverage instrumentation needed on the harness
# itself, matching OSAL's own convention.
add_executable(coverage-freertos-tasks-testrunner
    src/main.c
    src/coveragetest-tasks.c
    stubs/freertos-task-stubs.c
    stubs/osal-shared-stubs.c
    stubs/ut-bsp-stubs.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utassert.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utlist.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/uttools.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utstubs.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/uttest.c
    $<TARGET_OBJECTS:utobj_freertos_tasks>
)
target_include_directories(coverage-freertos-tasks-testrunner PRIVATE
    ${FREERTOS_COVERAGE_INCLUDES}
)
target_compile_definitions(coverage-freertos-tasks-testrunner PRIVATE "_UNIT_TEST_")
target_compile_options(coverage-freertos-tasks-testrunner PRIVATE --coverage)
target_link_options(coverage-freertos-tasks-testrunner PRIVATE --coverage)

add_test(NAME coverage-freertos-tasks COMMAND coverage-freertos-tasks-testrunner)
