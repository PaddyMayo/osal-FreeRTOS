# Short names for the cmake commands in CLAUDE.md, .pre-commit-config.yaml and
# the workflows. Run `make help` for the list.
#
#   make build  <platform...|all>      configure and build the osal library
#   make clean  <platform...|all>      remove the build directory of a platform
#   make test   [whitebox|blackbox|coverage]
#
# Make treats every word as a goal of its own, so the first word is the command
# and the rest are its targets, which have empty rules further down.

PLATFORMS    := posix posix-aarch64 cortex-m0 cortex-m3 cortex-m4f cortex-m7 cortex-m33
TEST_TARGETS := whitebox blackbox coverage
COMMANDS     := build clean test help

COMMAND := $(firstword $(MAKECMDGOALS))
TARGETS := $(filter-out $(COMMAND),$(MAKECMDGOALS))

.DEFAULT_GOAL := help

# One cmake at a time, each already runs its own jobs
.NOTPARALLEL:

.PHONY: $(COMMANDS) $(PLATFORMS) all $(TEST_TARGETS)

# ---- Check the command line before anything runs ----------------------------

USAGE := Use: make build <platform...|all>, make clean <platform...|all>, make test [$(TEST_TARGETS)]. Platforms: $(PLATFORMS)

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
	@echo 'make test                      run whitebox, then blackbox'
	@echo 'make test whitebox             coverage tests of freertos/ (freertos-coverage-tests/)'
	@echo 'make test blackbox             the OSAL tests, built and run on the host'
	@echo 'make test coverage             whitebox, then the 100% lines/branches gate'
	@echo
	@echo 'platforms: $(PLATFORMS)'
	@echo 'tests run on posix; the other platforms only build'

build: $(addprefix _build-,$(if $(filter all,$(TARGETS)),$(PLATFORMS),$(TARGETS)))

clean: $(addprefix _clean-,$(if $(filter all,$(TARGETS)),$(PLATFORMS),$(TARGETS)))

test: $(addprefix _test-,$(if $(TARGETS),$(TARGETS),whitebox blackbox))

# The words after the command are only read above
$(PLATFORMS) all $(TEST_TARGETS): ; @:

# ---- Work, one platform or test at a time -------------------------------------

# Only the host build runs the tests; the cross builds leave them off, as the
# freertos-cross-* pre-commit hooks do
_build-%:
	cmake -S . -B build/$* -C platforms/$*/config.cmake $(if $(filter posix,$*),,-DENABLE_UNIT_TESTS=OFF)
	cmake --build build/$* --target osal -j

_clean-%:
	rm -rf build/$*

POSIX_CONFIGURE := cmake -S . -B build/posix -C platforms/posix/config.cmake

# The freertos-coverage-* pre-commit hooks. Counters from an earlier run would
# hide a test that stopped covering, so they go before the tests run.
_test-whitebox:
	$(POSIX_CONFIGURE)
	cmake --build build/posix --target freertos_coverage_tests -j
	find build/posix/freertos-coverage-tests -name '*.gcda' -delete
	ctest --test-dir build/posix -R '^coverage-freertos-' --output-on-failure

# Everything that is not a coverage test; no --target, so the test programs are built
_test-blackbox:
	$(POSIX_CONFIGURE)
	cmake --build build/posix -j
	ctest --test-dir build/posix -E '^coverage-' --output-on-failure

_test-coverage: _test-whitebox
	gcovr build/posix/freertos-coverage-tests
