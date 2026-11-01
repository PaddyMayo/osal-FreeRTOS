# Short names for the cmake commands in CLAUDE.md, .pre-commit-config.yaml and
# the workflows. Run `make help` for the list.
#
#   make build  <platform...|all>      configure and build the osal library
#   make clean  <platform...|all>      remove the build directory of a platform
#   make test   [whitebox|blackbox|coverage]
#   make misra  <platform...|all>      MISRA C:2012 check of the port's code
#
# Make treats every word as a goal of its own, so the first word is the command
# and the rest are its targets, which have empty rules further down.

PLATFORMS    := posix-aarch64 cortex-m0 cortex-m3 cortex-m4f cortex-m7 cortex-m33
TEST_TARGETS := whitebox blackbox coverage
COMMANDS     := build clean test misra help

COMMAND := $(firstword $(MAKECMDGOALS))
TARGETS := $(filter-out $(COMMAND),$(MAKECMDGOALS))

.DEFAULT_GOAL := help

# One cmake at a time, each already runs its own jobs
.NOTPARALLEL:

.PHONY: $(COMMANDS) $(PLATFORMS) all $(TEST_TARGETS)

# ---- Check the command line before anything runs ----------------------------

USAGE := Use: make build <platform...|all>, make clean <platform...|all>, make misra <platform...|all>, make test [$(TEST_TARGETS)]. Platforms: $(PLATFORMS)

ifneq ($(MAKECMDGOALS),)
  ifeq ($(filter $(COMMAND),$(COMMANDS)),)
    $(error '$(COMMAND)' is not a command. $(USAGE))
  endif

  ifeq ($(COMMAND),help)
    ifneq ($(TARGETS),)
      $(error help takes no target. $(USAGE))
    endif
  endif

  ifeq ($(COMMAND),build)
    ifeq ($(TARGETS),)
      $(error build needs a target. $(USAGE))
    endif
    ifneq ($(filter-out $(PLATFORMS) all,$(TARGETS)),)
      $(error '$(filter-out $(PLATFORMS) all,$(TARGETS))' cannot be built. $(USAGE))
    endif
  endif

  ifeq ($(COMMAND),clean)
    ifeq ($(TARGETS),)
      $(error clean needs a target. $(USAGE))
    endif
    ifneq ($(filter-out $(PLATFORMS) all,$(TARGETS)),)
      $(error '$(filter-out $(PLATFORMS),$(TARGETS))' cannot be cleaned. $(USAGE))
    endif
  endif

  ifeq ($(COMMAND),misra)
    ifeq ($(TARGETS),)
      $(error misra needs a target. $(USAGE))
    endif
    ifneq ($(filter-out $(PLATFORMS) all,$(TARGETS)),)
      $(error '$(filter-out $(PLATFORMS) all,$(TARGETS))' cannot be checked. $(USAGE))
    endif
  endif

  ifeq ($(COMMAND),test)
    ifneq ($(filter-out $(TEST_TARGETS),$(TARGETS)),)
      $(error '$(filter-out $(TEST_TARGETS),$(TARGETS))' is not a test. $(USAGE))
    endif
  endif
endif

# ---- Commands -----------------------------------------------------------------

help:
	@echo 'make build <platform...|all>   configure and build the osal library'
	@echo 'make clean <platform...|all>   remove build/<platform> for each, nothing else'
	@echo 'make misra <platform...|all>   MISRA C:2012 check of freertos/, generic-freertos/ and platforms/'
	@echo 'make test                      run whitebox, then blackbox'
	@echo 'make test whitebox             coverage tests of freertos/ (freertos-coverage-tests/)'
	@echo 'make test blackbox             the OSAL tests, run on posix-aarch64 under qemu'
	@echo 'make test coverage             whitebox, then the 100% lines/branches gate'
	@echo
	@echo 'platforms: $(PLATFORMS)'
	@echo 'tests run on posix-aarch64, under qemu; the other platforms only build'

build: $(addprefix _build-,$(if $(filter all,$(TARGETS)),$(PLATFORMS),$(TARGETS)))

clean: $(addprefix _clean-,$(if $(filter all,$(TARGETS)),$(PLATFORMS),$(TARGETS)))

misra: $(addprefix _misra-,$(if $(filter all,$(TARGETS)),$(PLATFORMS),$(TARGETS)))

test: $(addprefix _test-,$(if $(TARGETS),$(TARGETS),whitebox blackbox))

# The words after the command are only read above
$(PLATFORMS) all $(TEST_TARGETS): ; @:

# ---- Work, one platform or test at a time -------------------------------------

# Only posix-aarch64 builds the tests, and ctest runs each of them under
# qemu-aarch64; the other builds leave them off, as the freertos-cross-*
# pre-commit hooks do
_configure-%:
	cmake -S . -B build/$* -C platforms/$*/config.cmake $(if $(filter posix-aarch64,$*),'-DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64;-L;/usr/aarch64-linux-gnu',-DENABLE_UNIT_TESTS=OFF)

_build-%: _configure-%
	cmake --build build/$* --target osal -j

_clean-%:
	rm -rf build/$*

# MISRA C:2012 on the port's own code, as the platform builds it: cppcheck reads the
# defines and include paths of each file from the build's compile_commands.json, so
# a platform is checked with the code its own #if branches select. Any violation
# fails; misra-suppressions.txt lists the rules the port breaks today.
#
# compile_commands.json is cut down first:
#  - to the files of freertos/, generic-freertos/ and platforms/. Without it the
#    coverage tests (posix-aarch64), which compile the same sources against fake kernel
#    headers, and the OSAL and kernel sources are checked too.
#  - -isystem becomes -I. cppcheck leaves the -isystem paths out, which loses
#    FreeRTOSConfig.h, and a file whose include fails is skipped with an error
#    that points into third_party/ and so is not reported.
#
# The Cortex-M platforms are 32-bit (int, long and pointers), which cppcheck does
# not know from the compile command, so it gets --platform=unix32 for them.
#
# misra-config is suppressed: it only says a name could not be resolved, and what is
# left unresolved is the console's vendor macros (bsp_console.c), which no one build
# defines. The rules are still checked on everything it did parse.
_misra-%: _configure-%
	jq --arg root '$(CURDIR)' \
	  '[.[] | select((.file | test("^" + $$root + "/(freertos|generic-freertos|platforms)/")) and (.directory | contains("/freertos-coverage-tests") | not)) | .command |= gsub("-isystem "; "-I")]' \
	  build/$*/compile_commands.json > build/$*/compile_commands.misra.json
	cppcheck --project=build/$*/compile_commands.misra.json -D__GNUC__ $(if $(filter cortex-%,$*),--platform=unix32) \
	  --addon=misra --enable=style --error-exitcode=1 --quiet --inline-suppr \
	  --suppressions-list=misra-suppressions.txt \
	  --suppress=missingIncludeSystem --suppress=misra-config

# The freertos-coverage-* pre-commit hooks. Counters from an earlier run would
# hide a test that stopped covering, so they go before the tests run.
_test-whitebox: _configure-posix-aarch64
	cmake --build build/posix-aarch64 --target freertos_coverage_tests -j
	find build/posix-aarch64/freertos-coverage-tests -name '*.gcda' -delete
	ctest --test-dir build/posix-aarch64 -R '^coverage-freertos-' --output-on-failure

# Everything that is not a coverage test; no --target, so the test programs are built
_test-blackbox: _configure-posix-aarch64
	cmake --build build/posix-aarch64 -j
	ctest --test-dir build/posix-aarch64 -E '^coverage-' --output-on-failure

_test-coverage: _test-whitebox
	gcovr build/posix-aarch64/freertos-coverage-tests
