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
# Modules: FREERTOS_MODULE_LIST below names every module under test, and a
# coverage test target is generated for each. Adding a new module's coverage
# test means adding its src/coveragetest-<module>.c, naming it in the list,
# and - if the module needs them - its own FreeRTOS API fakes in
# stubs/freertos-<module>-stubs.c and/or test-only accessors into the
# module's internal state in adaptors/src/ut-adaptor-<module>.c.
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

# Modules that take their shared-layer dependencies (OS_SharedGlobalVars, the
# OS_*_table arrays, OS_TaskEntryPoint, ...) from OSAL's own ut_osapi_*_stubs
# libraries, as OSAL's unit-test-coverage does, instead of the hand-written
# stubs/osal-shared-stubs.c.
set(FREERTOS_OSAL_STUB_MODULES queue)

# Umbrella target aggregating every discovered module's testrunner below, so
# CI (and anyone else) can build "all coverage tests that currently exist"
# without naming any specific module.
add_custom_target(freertos_coverage_tests)

# A list of modules in the FreeRTOS OSAL implementation.
# Each entry corresponds to a source file name freertos/src/os-impl-${MODULE}.c
# and a test case src/coveragetest-${MODULE}.c
set(FREERTOS_MODULE_LIST
    common
    queue
    tasks
)

foreach(MODULE_NAME ${FREERTOS_MODULE_LIST})
    set(IMPL_SRC ${CMAKE_SOURCE_DIR}/freertos/src/os-impl-${MODULE_NAME}.c)
    set(COVERAGETEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/coveragetest-${MODULE_NAME}.c)
    foreach(REQUIRED_SRC ${IMPL_SRC} ${COVERAGETEST_SRC})
        if (NOT EXISTS ${REQUIRED_SRC})
            message(FATAL_ERROR "FREERTOS_MODULE_LIST names ${MODULE_NAME}, but ${REQUIRED_SRC} does not exist")
        endif ()
    endforeach()

    set(UTOBJ_TARGET utobj_freertos_${MODULE_NAME})
    set(TESTRUNNER_TARGET coverage-freertos-${MODULE_NAME}-testrunner)

    # The file under test, compiled with gcov instrumentation and pointed at
    # our fake FreeRTOS headers (fake-inc) instead of the real kernel.
    add_library(${UTOBJ_TARGET} OBJECT ${IMPL_SRC})
    target_include_directories(${UTOBJ_TARGET} BEFORE PRIVATE fake-inc)
    target_include_directories(${UTOBJ_TARGET} PRIVATE ${FREERTOS_COVERAGE_INCLUDES})
    target_compile_options(${UTOBJ_TARGET} PRIVATE --coverage)

    set(MODULE_SOURCES ${FREERTOS_COVERAGE_COMMON_SOURCES} ${COVERAGETEST_SRC})
    if (MODULE_NAME IN_LIST FREERTOS_OSAL_STUB_MODULES)
        list(REMOVE_ITEM MODULE_SOURCES stubs/osal-shared-stubs.c)
    endif ()

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
    if (MODULE_NAME IN_LIST FREERTOS_OSAL_STUB_MODULES)
        # Linked by file, not by target: the targets' INTERFACE chain
        # (ut_osapi_stub_headers -> ut_assert -> osal_bsp -> freertos_kernel)
        # would add the board linker script and the real BSP/kernel.
        add_dependencies(${TESTRUNNER_TARGET} ut_osapi_table_stubs ut_osapi_shared_stubs)
        target_link_libraries(${TESTRUNNER_TARGET} PRIVATE
            $<TARGET_FILE:ut_osapi_table_stubs>
            $<TARGET_FILE:ut_osapi_shared_stubs>
        )
    endif ()
    target_compile_definitions(${TESTRUNNER_TARGET} PRIVATE "_UNIT_TEST_")
    target_compile_options(${TESTRUNNER_TARGET} PRIVATE --coverage)
    target_link_options(${TESTRUNNER_TARGET} PRIVATE --coverage)

    add_test(NAME coverage-freertos-${MODULE_NAME} COMMAND ${TESTRUNNER_TARGET})
    add_dependencies(freertos_coverage_tests ${TESTRUNNER_TARGET})
endforeach()

# The BSP's files (generic-freertos/src/bsp_<name>.c) are not os-impl modules,
# so they sit outside FREERTOS_MODULE_LIST and follow the same pattern from
# their own list: each entry needs src/coveragetest-bsp-<name>.c, and may add
# stubs/freertos-bsp-<name>-stubs.c. bsp_console.c is not listed yet: it has no
# tests (see the TODO on OS_FreeRTOS_AssertFailed).
#
# The real file is compiled against the same fake FreeRTOS headers, with the
# common sources that do not need the OSAL shared layer, plus OSAL's own
# bsp/shared osapi-bsp.c for OS_BSP_Global and OS_BSP_SetExitCode. A file's
# main() is built as OS_BSP_TestMain, so it does not collide with src/main.c.
set(FREERTOS_BSP_FILE_LIST
    start
)

set(BSP_COMMON_SOURCES ${FREERTOS_COVERAGE_COMMON_SOURCES})
list(REMOVE_ITEM BSP_COMMON_SOURCES stubs/osal-shared-stubs.c)
list(APPEND BSP_COMMON_SOURCES ${OSAL_REAL_SOURCE_DIR}/src/bsp/shared/src/osapi-bsp.c)

foreach(BSP_NAME ${FREERTOS_BSP_FILE_LIST})
    set(BSP_SRC ${CMAKE_SOURCE_DIR}/generic-freertos/src/bsp_${BSP_NAME}.c)
    set(BSP_COVERAGETEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/coveragetest-bsp-${BSP_NAME}.c)
    foreach(REQUIRED_SRC ${BSP_SRC} ${BSP_COVERAGETEST_SRC})
        if (NOT EXISTS ${REQUIRED_SRC})
            message(FATAL_ERROR "FREERTOS_BSP_FILE_LIST names ${BSP_NAME}, but ${REQUIRED_SRC} does not exist")
        endif ()
    endforeach()

    set(BSP_TESTRUNNER_TARGET coverage-bsp-${BSP_NAME}-testrunner)
    set(BSP_SOURCES ${BSP_COMMON_SOURCES} ${BSP_COVERAGETEST_SRC} ${BSP_SRC})

    set(BSP_STUB ${CMAKE_CURRENT_SOURCE_DIR}/stubs/freertos-bsp-${BSP_NAME}-stubs.c)
    if (EXISTS ${BSP_STUB})
        list(APPEND BSP_SOURCES ${BSP_STUB})
    endif ()

    set_source_files_properties(${BSP_SRC} PROPERTIES COMPILE_DEFINITIONS "main=OS_BSP_TestMain")

    add_executable(${BSP_TESTRUNNER_TARGET} ${BSP_SOURCES})
    target_include_directories(${BSP_TESTRUNNER_TARGET} BEFORE PRIVATE fake-inc)
    target_include_directories(${BSP_TESTRUNNER_TARGET} PRIVATE
        ${FREERTOS_COVERAGE_INCLUDES}
        ${OSAL_REAL_SOURCE_DIR}/src/bsp/shared/inc
    )
    target_compile_definitions(${BSP_TESTRUNNER_TARGET} PRIVATE "_UNIT_TEST_")
    target_compile_options(${BSP_TESTRUNNER_TARGET} PRIVATE --coverage)
    target_link_options(${BSP_TESTRUNNER_TARGET} PRIVATE --coverage)

    # Named coverage-freertos-* so the freertos-coverage-test hook (ctest -R '^coverage-freertos-') runs it
    add_test(NAME coverage-freertos-bsp-${BSP_NAME} COMMAND ${BSP_TESTRUNNER_TARGET})
    add_dependencies(freertos_coverage_tests ${BSP_TESTRUNNER_TARGET})
endforeach()

# Every freertos/src/os-impl-<module>.c must be in FREERTOS_MODULE_LIST, and so
# have a coverage test: gcovr only sees files some test executable compiled, so
# an unlisted module would otherwise drop out of the 100% gate unnoticed. The
# failing test is only generated when there is such a module.
#
# FREERTOS_STUB_MODULES are exempt: they only return OS_ERR_NOT_IMPLEMENTED (or
# are no-ops), so there is nothing to cover. Remove a module from this list
# when it gains a real implementation, and the check then requires its test.
set(FREERTOS_STUB_MODULES
    binsem
    clock
    condvar
    console
    countsem
    dir
    errors
    file
    filesys
    heap
    idmap
    module
    mutex
    network
    rwlock
    select
    shell
    sockets
    timebase
)
file(GLOB FREERTOS_IMPL_SOURCES ${CMAKE_SOURCE_DIR}/freertos/src/os-impl-*.c)
set(FREERTOS_UNTESTED_MODULES)
foreach(IMPL_SRC ${FREERTOS_IMPL_SOURCES})
    get_filename_component(IMPL_NAME ${IMPL_SRC} NAME_WE)
    string(REPLACE "os-impl-" "" MODULE_NAME ${IMPL_NAME})
    if (NOT MODULE_NAME IN_LIST FREERTOS_MODULE_LIST AND NOT MODULE_NAME IN_LIST FREERTOS_STUB_MODULES)
        list(APPEND FREERTOS_UNTESTED_MODULES ${MODULE_NAME})
    endif ()
endforeach()

if (FREERTOS_UNTESTED_MODULES)
    list(JOIN FREERTOS_UNTESTED_MODULES " " FREERTOS_UNTESTED_MODULES_TEXT)
    add_test(NAME coverage-freertos-module-list
        COMMAND sh -c "echo 'No white-box coverage test for: ${FREERTOS_UNTESTED_MODULES_TEXT}. Add freertos-coverage-tests/src/coveragetest-<module>.c and name each in FREERTOS_MODULE_LIST.' >&2; exit 1"
    )
endif ()
