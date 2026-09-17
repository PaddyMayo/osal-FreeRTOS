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
# Module discovery: for every freertos/src/os-impl-<module>.c, if a
# matching src/coveragetest-<module>.c exists here, a coverage test target
# is generated automatically. Adding a new module's coverage test is just
# dropping in those two files, plus - if the module needs them - its own
# FreeRTOS API fakes in stubs/freertos-<module>-stubs.c and/or test-only
# accessors into the module's internal state in
# adaptors/src/ut-adaptor-<module>.c. Nothing below needs editing.
#
##########################################################################

set(OSAL_REAL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/osal)
set(UT_ASSERT_REAL_SOURCE_DIR ${OSAL_REAL_SOURCE_DIR}/ut_assert)

set(FREERTOS_COVERAGE_INCLUDES
    fake-inc
    adaptors/inc
    ${CMAKE_SOURCE_DIR}/freertos/inc
    ${OSAL_REAL_SOURCE_DIR}/src/os/inc
    ${OSAL_REAL_SOURCE_DIR}/src/os/shared/inc
    ${CMAKE_BINARY_DIR}/osal_build/inc
    ${UT_ASSERT_REAL_SOURCE_DIR}/inc
)

# Sources every module's test executable needs regardless of which OSAL
# subsystem it targets: the plain (non-scheduler) main(), the OS_task_table/
# OS_TaskEntryPoint-style shared-layer stand-ins, the minimal UT_BSP_*
# implementation, and ut_assert's own OS-agnostic sources.
set(FREERTOS_COVERAGE_COMMON_SOURCES
    src/main.c
    stubs/osal-shared-stubs.c
    stubs/ut-bsp-stubs.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utassert.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utlist.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/uttools.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/utstubs.c
    ${UT_ASSERT_REAL_SOURCE_DIR}/src/uttest.c
)

# Umbrella target aggregating every discovered module's testrunner below, so
# CI (and anyone else) can build "all coverage tests that currently exist"
# without naming any specific module.
add_custom_target(freertos_coverage_tests)

file(GLOB FREERTOS_IMPL_SOURCES ${CMAKE_SOURCE_DIR}/freertos/src/os-impl-*.c)

foreach(IMPL_SRC ${FREERTOS_IMPL_SOURCES})
    get_filename_component(IMPL_NAME ${IMPL_SRC} NAME_WE)
    string(REPLACE "os-impl-" "" MODULE_NAME ${IMPL_NAME})

    set(COVERAGETEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/coveragetest-${MODULE_NAME}.c)
    if (NOT EXISTS ${COVERAGETEST_SRC})
        continue()
    endif ()

    set(UTOBJ_TARGET utobj_freertos_${MODULE_NAME})
    set(TESTRUNNER_TARGET coverage-freertos-${MODULE_NAME}-testrunner)

    # The file under test, compiled with gcov instrumentation and pointed at
    # our fake FreeRTOS headers (fake-inc) instead of the real kernel.
    add_library(${UTOBJ_TARGET} OBJECT ${IMPL_SRC})
    target_include_directories(${UTOBJ_TARGET} BEFORE PRIVATE fake-inc)
    target_include_directories(${UTOBJ_TARGET} PRIVATE ${FREERTOS_COVERAGE_INCLUDES})
    target_compile_options(${UTOBJ_TARGET} PRIVATE --coverage)

    set(MODULE_SOURCES ${FREERTOS_COVERAGE_COMMON_SOURCES} ${COVERAGETEST_SRC})

    set(MODULE_STUB ${CMAKE_CURRENT_SOURCE_DIR}/stubs/freertos-${MODULE_NAME}-stubs.c)
    if (EXISTS ${MODULE_STUB})
        list(APPEND MODULE_SOURCES ${MODULE_STUB})
    endif ()

    set(MODULE_ADAPTOR ${CMAKE_CURRENT_SOURCE_DIR}/adaptors/src/ut-adaptor-${MODULE_NAME}.c)
    if (EXISTS ${MODULE_ADAPTOR})
        list(APPEND MODULE_SOURCES ${MODULE_ADAPTOR})
    endif ()

    add_executable(${TESTRUNNER_TARGET}
        ${MODULE_SOURCES}
        $<TARGET_OBJECTS:${UTOBJ_TARGET}>
    )
    target_include_directories(${TESTRUNNER_TARGET} PRIVATE ${FREERTOS_COVERAGE_INCLUDES})
    target_compile_definitions(${TESTRUNNER_TARGET} PRIVATE "_UNIT_TEST_")
    target_compile_options(${TESTRUNNER_TARGET} PRIVATE --coverage)
    target_link_options(${TESTRUNNER_TARGET} PRIVATE --coverage)

    add_test(NAME coverage-freertos-${MODULE_NAME} COMMAND ${TESTRUNNER_TARGET})
    add_dependencies(freertos_coverage_tests ${TESTRUNNER_TARGET})
endforeach()
