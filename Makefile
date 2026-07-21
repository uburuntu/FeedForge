# FeedForge developer command surface.
#
# This is a thin convenience layer. CMakePresets.json and CMake remain the
# authoritative build graph and package interface.

.DEFAULT_GOAL := help
SHELL := /bin/sh
.SUFFIXES:
.DELETE_ON_ERROR:

override MAKEFILE_PATH := $(abspath $(firstword $(MAKEFILE_LIST)))
override ROOT := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
ifneq ($(CURDIR),$(ROOT))
$(error Run FeedForge Make from $(ROOT), or use make -C "$(ROOT)")
endif
override BUILD_ROOT := $(ROOT)/build
override OUT_ROOT := $(ROOT)/out
override TIDY_LOG := $(OUT_ROOT)/tidy/clang-tidy.log

CMAKE ?= cmake
CTEST ?= ctest
PYTHON ?= python3
GIT ?= git
DOCKER ?= docker
NINJA ?= ninja
HOST_CXX ?= c++
override CMAKE_BUILD = MAKEFLAGS= MFLAGS= "$(CMAKE)" --build

PRESET ?= fast
JOBS ?=
CMAKE_ARGS ?=
BUILD_ARGS ?=
CTEST_ARGS ?=
COLOR ?= auto
CONFIRM ?=
FORCE ?= 0

INVALID_CMAKE_LAYOUT_ARGS := $(filter -B% -S% --preset%,$(CMAKE_ARGS))
ifneq ($(strip $(INVALID_CMAKE_LAYOUT_ARGS)),)
$(error CMAKE_ARGS may set configure options, but not -B, -S, or --preset)
endif

PARALLEL = $(if $(strip $(JOBS)),--parallel $(JOBS),)

GENERATE_PRESET ?= dev
INSTALL_PRESET ?= release
PREFIX ?= $(BUILD_ROOT)/install
RUNTIME_PREFIX ?= $(BUILD_ROOT)/runtime-install

SCHEMA ?= schemas/nasdaq/totalview_itch_5_0.toml
PIPELINE ?= pipelines/order_events.toml
GENERATED_OUTPUT ?= $(BUILD_ROOT)/manual/itch50_order_events.hpp
IR_OUTPUT ?= $(BUILD_ROOT)/manual/itch50_order_events.ffir.json
REPLAY_FILE ?=
FEEDFORGEC = $(BUILD_ROOT)/$(GENERATE_PRESET)/src/feedforgec/feedforgec

LLVM_PREFIX = $(shell \
	if command -v brew >/dev/null 2>&1 && brew --prefix llvm >/dev/null 2>&1; then \
		brew --prefix llvm; \
	elif command -v llvm-config >/dev/null 2>&1; then \
		llvm-config --prefix; \
	fi)
LLVM_CXX ?= $(if $(strip $(LLVM_PREFIX)),$(LLVM_PREFIX)/bin/clang++,clang++)
CLANG_FORMAT ?= $(if $(strip $(LLVM_PREFIX)),$(LLVM_PREFIX)/bin/clang-format,clang-format)
CLANG_TIDY ?= $(if $(strip $(LLVM_PREFIX)),$(LLVM_PREFIX)/bin/clang-tidy,clang-tidy)
RUN_CLANG_TIDY ?= $(if $(strip $(LLVM_PREFIX)),$(LLVM_PREFIX)/bin/run-clang-tidy,run-clang-tidy)
GIT_CLANG_FORMAT ?= $(if $(strip $(LLVM_PREFIX)),$(LLVM_PREFIX)/bin/git-clang-format,git-clang-format)

LLVM_DEV_DIR ?= $(BUILD_ROOT)/dev-llvm
LLVM_SANITIZER_DIR ?= $(BUILD_ROOT)/sanitizers-llvm
RTSAN_BUILD_DIR ?= $(BUILD_ROOT)/rtsan-llvm
FUZZ_BUILD_DIR ?= $(BUILD_ROOT)/fuzz-llvm

UNAME_S = $(shell uname -s)
UNAME_M = $(shell uname -m)
ASAN_DETECT_LEAKS ?= $(if $(filter Darwin,$(UNAME_S)),0,1)
ASAN_OPTIONS ?= detect_leaks=$(ASAN_DETECT_LEAKS):abort_on_error=1
UBSAN_OPTIONS ?= print_stacktrace=1:halt_on_error=1

FUZZ_SECONDS ?= 15
FUZZ_SEED ?= 300
FUZZ_MAX_LEN ?= 4096
FUZZ_TIMEOUT ?= 5
FUZZ_RUN_ROOT ?= $(OUT_ROOT)/fuzz
FUZZ_BIN_DIR = $(FUZZ_BUILD_DIR)/fuzz
FUZZ_SEED_DIR = $(FUZZ_BIN_DIR)/corpus

BENCH_LABEL ?=
BENCH_SOURCE_ID ?=
BENCH_OUTPUT_DIR ?= $(BUILD_ROOT)/bench/results/$(BENCH_LABEL)
BENCH_EXECUTABLE ?= $(BUILD_ROOT)/bench/benchmarks/feedforge_benchmark
BENCH_BASELINE ?=
BENCH_CANDIDATE ?=
BENCH_TARGETS ?=
BENCH_COMPARE_JSON ?= $(BUILD_ROOT)/bench/results/comparison.json
BENCH_COMPARE_CSV ?= $(BUILD_ROOT)/bench/results/comparison.csv
BENCH_TARGET_ARGS = $(foreach target,$(BENCH_TARGETS),--target "$(target)")

RELEASE_REVISION ?= $(shell $(GIT) rev-parse HEAD 2>/dev/null)
RELEASE_OUTPUT_DIR ?= $(OUT_ROOT)/release

LINUX_IMAGE ?= ubuntu:24.04
DOCKER_ARCH = $(if $(filter arm64 aarch64,$(UNAME_M)),arm64,$(if $(filter x86_64 amd64,$(UNAME_M)),amd64,$(UNAME_M)))
DOCKER_PLATFORM ?= linux/$(DOCKER_ARCH)
LINUX_JOBS ?= $(if $(strip $(JOBS)),$(JOBS),4)
TIDY_JOBS ?= $(if $(strip $(JOBS)),$(JOBS),4)

FORMAT_PATHS := include src tests fuzz benchmarks examples
FORMAT_EXTENSIONS := h,hpp,c,cc,cpp,cxx
FORMAT_EXCLUDES := ':(exclude)tests/golden/synthetic_pipeline.hpp'

define announce
@set -eu; \
	mode="$(COLOR)"; \
	if test -n "$${NO_COLOR:-}"; then mode=never; fi; \
	case "$$mode" in \
		always) printf '\033[1;36m==>\033[0m %s\n' "$(1)" ;; \
		never) printf '==> %s\n' "$(1)" ;; \
		auto) \
			if test -t 1 && test "$${TERM:-dumb}" != dumb; then \
				printf '\033[1;36m==>\033[0m %s\n' "$(1)"; \
			else \
				printf '==> %s\n' "$(1)"; \
			fi ;; \
		*) printf 'COLOR must be auto, always, or never\n' >&2; exit 2 ;; \
	esac
endef

define require_llvm
@command -v "$(LLVM_CXX)" >/dev/null 2>&1 || { \
	printf 'Upstream clang++ not found. Set LLVM_CXX or install Homebrew LLVM.\n' >&2; \
	exit 2; \
}
endef

define guard_build_output
@"$(CMAKE)" \
	"-DFEEDFORGE_PROJECT_ROOT=$(ROOT)" \
	"-DFEEDFORGE_BUILD_ROOT=$(BUILD_ROOT)" \
	"-DFEEDFORGE_OUTPUT_PATH=$(1)" \
	-P "$(ROOT)/cmake/FeedForgeOutputGuard.cmake"
endef

define run_fuzzer
@set -eu; \
	case "$(FUZZ_SECONDS)" in ''|*[!0-9]*) \
		printf 'FUZZ_SECONDS must be a positive integer\n' >&2; exit 2 ;; esac; \
	test "$(FUZZ_SECONDS)" -gt 0 || { printf 'FUZZ_SECONDS must be positive\n' >&2; exit 2; }; \
	work="$(FUZZ_RUN_ROOT)/$(1)/corpus"; \
	artifacts="$(FUZZ_RUN_ROOT)/$(1)/artifacts"; \
	mkdir -p "$$work" "$$artifacts"; \
	printf 'Fuzzing %s for %ss (seed %s)\n' "$(1)" "$(FUZZ_SECONDS)" "$(FUZZ_SEED)"; \
	ASAN_OPTIONS="$(ASAN_OPTIONS)" UBSAN_OPTIONS="$(UBSAN_OPTIONS)" \
		"$(FUZZ_BIN_DIR)/fuzz_$(1)" \
		"$$work" "$(FUZZ_SEED_DIR)/$(1)" \
		-seed="$(FUZZ_SEED)" \
		-max_total_time="$(FUZZ_SECONDS)" \
		-max_len="$(FUZZ_MAX_LEN)" \
		-timeout="$(FUZZ_TIMEOUT)" \
		-verbosity=0 \
		-print_final_stats=1 \
		-artifact_prefix="$$artifacts/"
endef

.PHONY: \
	help doctor status presets variables \
	configure build test check quick dev release sanitizers compiler-off no-exceptions-rtti \
	verify verify-all \
	test-allocation test-arbitrary-input test-installed \
	generated-check generated-refresh compiler validate pipeline-compile pipeline-ir \
	demo replay replay-empty \
	llvm-dev llvm-sanitizers rtsan \
	fuzz-build fuzz-binary-file fuzz-decode-one fuzz-differential-decode fuzz-replay fuzz-smoke \
	bench-smoke bench-run bench-compare \
	release-assets release-assets-check install install-runtime \
	lint format-check format-changed tidy linux-smoke \
	clean clobber

##@ Start here

help: ## Show the project command catalog (default)
	@set -eu; \
	mode="$(COLOR)"; \
	if test -n "$${NO_COLOR:-}"; then mode=never; fi; \
	case "$$mode" in \
		always) use_color=1 ;; \
		never) use_color=0 ;; \
		auto) if test -t 1 && test "$${TERM:-dumb}" != dumb; then use_color=1; else use_color=0; fi ;; \
		*) printf 'COLOR must be auto, always, or never\n' >&2; exit 2 ;; \
	esac; \
	awk -v color="$$use_color" 'BEGIN { FS = ":.*##"; \
		if (color) { bold="\033[1m"; cyan="\033[36m"; dim="\033[2m"; reset="\033[0m" } \
		printf "\n%sFeedForge%s\n", bold, reset; \
		printf "%sThin developer wrapper; CMakePresets.json remains authoritative.%s\n", dim, reset } \
		/^##@/ { section=$$0; sub(/^##@[[:space:]]*/, "", section); \
			printf "\n%s%s%s\n", cyan, section, reset; next } \
		/^[A-Za-z0-9_.-]+:.*##/ { printf "  %s%-24s%s %s\n", bold, $$1, reset, $$2 } \
		END { printf "\nRun %smake variables%s for configurable paths and flags.\n\n", bold, reset }' \
		$(MAKEFILE_LIST)

doctor: ## Check required tools and report optional local capabilities
	$(call announce,Checking the development environment)
	@set -eu; \
	missing=0; \
	for tool in "$(CMAKE)" "$(CTEST)" "$(NINJA)" "$(GIT)" "$(HOST_CXX)"; do \
		if command -v "$$tool" >/dev/null 2>&1; then \
			printf '  [ok] %-16s %s\n' "$$tool" "$$(command -v "$$tool")"; \
		else \
			printf '  [missing] %s\n' "$$tool"; missing=1; \
		fi; \
	done; \
	if command -v "$(CMAKE)" >/dev/null 2>&1; then \
		cmake_line="$$($(CMAKE) --version | sed -n '1p')"; \
		printf '%s\n' "$$cmake_line"; \
		cmake_version="$${cmake_line##* }"; \
		cmake_major="$${cmake_version%%.*}"; \
		cmake_rest="$${cmake_version#*.}"; \
		cmake_minor="$${cmake_rest%%.*}"; \
		case "$$cmake_major:$$cmake_minor" in \
			*[!0-9:]*|:*) printf '  [unsupported] cannot parse CMake version\n'; missing=1 ;; \
			*) if test "$$cmake_major" -lt 3 || { test "$$cmake_major" -eq 3 && test "$$cmake_minor" -lt 25; }; then \
				printf '  [unsupported] CMake 3.25 or newer is required\n'; missing=1; \
			fi ;; \
		esac; \
	fi; \
	if command -v "$(NINJA)" >/dev/null 2>&1; then "$(NINJA)" --version | sed 's/^/Ninja /'; fi; \
	if command -v "$(HOST_CXX)" >/dev/null 2>&1; then \
		"$(HOST_CXX)" --version | sed -n '1p'; \
		for standard in c++20 c++23; do \
			if printf 'int main() { return 0; }\n' | "$(HOST_CXX)" -std="$$standard" -x c++ -fsyntax-only - >/dev/null 2>&1; then \
				printf '  [ok] %-16s %s\n' "$$standard" "$(HOST_CXX)"; \
			else \
				printf '  [unsupported] %s with %s\n' "$$standard" "$(HOST_CXX)"; missing=1; \
			fi; \
		done; \
	fi; \
	if command -v "$(PYTHON)" >/dev/null 2>&1; then "$(PYTHON)" --version; else printf '  [optional] python3 missing (benchmark and release tooling unavailable)\n'; fi; \
	if command -v "$(LLVM_CXX)" >/dev/null 2>&1; then "$(LLVM_CXX)" --version | sed -n '1p'; else printf '  [optional] upstream LLVM missing (fuzz/RTSan unavailable)\n'; fi; \
	if command -v "$(DOCKER)" >/dev/null 2>&1; then "$(DOCKER)" --version; else printf '  [optional] Docker missing (linux-smoke unavailable)\n'; fi; \
	test "$$missing" -eq 0

status: ## Show Git state, compiler identity, and configured build trees
	$(call announce,Repository status)
	@$(GIT) status --short --branch
	@printf '\nDefault compiler:\n'
	@"$(HOST_CXX)" --version | sed -n '1,2p'
	@printf '\nConfigured build trees:\n'
	@find "$(BUILD_ROOT)" -mindepth 2 -maxdepth 2 -name CMakeCache.txt -print 2>/dev/null | sed 's#/CMakeCache.txt$$##' | sort || true

presets: ## List all shared and developer-local CMake presets
	@$(CMAKE) --list-presets=all

variables: ## Show the most useful Makefile overrides
	@printf '%-22s %s\n' \
		'PRESET' '$(PRESET)' \
		'JOBS' '$(JOBS)' \
		'PREFIX' '$(PREFIX)' \
		'HOST_CXX' '$(HOST_CXX)' \
		'LLVM_CXX' '$(LLVM_CXX)' \
		'SCHEMA' '$(SCHEMA)' \
		'PIPELINE' '$(PIPELINE)' \
		'FUZZ_SECONDS' '$(FUZZ_SECONDS)' \
		'BENCH_LABEL' '$(BENCH_LABEL)' \
		'BENCH_SOURCE_ID' '$(BENCH_SOURCE_ID)' \
		'RELEASE_REVISION' '$(RELEASE_REVISION)' \
		'RELEASE_OUTPUT_DIR' '$(RELEASE_OUTPUT_DIR)' \
		'DOCKER_PLATFORM' '$(DOCKER_PLATFORM)'

##@ Everyday builds

configure: ## Configure PRESET (default: fast)
	$(call announce,Configuring $(PRESET))
	@$(CMAKE) --preset "$(PRESET)" $(CMAKE_ARGS)

build: configure ## Configure and build PRESET
	$(call announce,Building $(PRESET))
	@$(CMAKE_BUILD) --preset "$(PRESET)" $(PARALLEL) $(BUILD_ARGS)

test: build ## Configure, build, and test PRESET
	$(call announce,Testing $(PRESET))
	@$(CTEST) --preset "$(PRESET)" $(CTEST_ARGS)

check: test ## Alias for the complete PRESET configure/build/test chain
	@:

quick: ## Run the focused fast suite
	+@$(MAKE) --no-print-directory check PRESET=fast

dev: ## Run the full Debug suite and verify committed generated headers
	+@$(MAKE) --no-print-directory check PRESET=dev
	+@$(MAKE) --no-print-directory generated-check GENERATE_PRESET=dev

release: ## Run the full Release suite and verify committed generated headers
	+@$(MAKE) --no-print-directory check PRESET=release
	+@$(MAKE) --no-print-directory generated-check GENERATE_PRESET=release

sanitizers: ## Run the shared Clang ASan+UBSan preset
	$(call announce,Configuring sanitizers)
	@$(CMAKE) --preset sanitizers $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset sanitizers $(PARALLEL) $(BUILD_ARGS)
	@ASAN_OPTIONS="$(ASAN_OPTIONS)" UBSAN_OPTIONS="$(UBSAN_OPTIONS)" \
		$(CTEST) --preset sanitizers $(CTEST_ARGS)

compiler-off: ## Validate strict C++20 runtime and committed headers without feedforgec
	+@$(MAKE) --no-print-directory check PRESET=compiler-off

no-exceptions-rtti: ## Validate generated/runtime code with exceptions and RTTI disabled
	+@$(MAKE) --no-print-directory check PRESET=no-exceptions-rtti

verify: ## Run the complete portable local correctness matrix (not hosted CI)
	$(call announce,Running the portable local verification matrix)
	+@$(MAKE) --no-print-directory quick
	+@$(MAKE) --no-print-directory dev
	+@$(MAKE) --no-print-directory release
	+@$(MAKE) --no-print-directory compiler-off
	+@$(MAKE) --no-print-directory no-exceptions-rtti
	+@$(MAKE) --no-print-directory sanitizers
	+@$(MAKE) --no-print-directory lint

verify-all: ## Extend verify with LLVM, RTSan, fuzz, formatting, and benchmarks
	$(call announce,Running the extended local verification matrix)
	+@$(MAKE) --no-print-directory verify
	+@$(MAKE) --no-print-directory llvm-dev
	+@$(MAKE) --no-print-directory llvm-sanitizers
	+@$(MAKE) --no-print-directory rtsan
	+@$(MAKE) --no-print-directory fuzz-smoke
	+@$(MAKE) --no-print-directory bench-smoke
	+@$(MAKE) --no-print-directory release-assets-check
	+@$(MAKE) --no-print-directory format-check

##@ Focused tests

test-allocation: ## Run the allocation-free hot-path test
	+@$(MAKE) --no-print-directory build PRESET=dev
	@$(CTEST) --test-dir "$(BUILD_ROOT)/dev" --output-on-failure -L allocation $(CTEST_ARGS)

test-arbitrary-input: ## Run deterministic arbitrary-input harnesses
	+@$(MAKE) --no-print-directory build PRESET=dev
	@$(CTEST) --test-dir "$(BUILD_ROOT)/dev" --output-on-failure -L arbitrary-input $(CTEST_ARGS)

test-installed: ## Run installed canonical and generated consumer tests
	+@$(MAKE) --no-print-directory build PRESET=dev
	@$(CTEST) --test-dir "$(BUILD_ROOT)/dev" --output-on-failure -L installed $(CTEST_ARGS)

##@ Generation and replay

demo: ## Build and run the embedded synthetic order-event showcase
	@set -eu; \
	log="$$(mktemp "$${TMPDIR:-/tmp}/feedforge-demo.XXXXXX")"; \
	trap 'rm -f "$$log"' EXIT HUP INT TERM; \
	if ! $(CMAKE) --preset compiler-off $(CMAKE_ARGS) >"$$log" 2>&1; then \
		cat "$$log" >&2; exit 1; \
	fi; \
	if ! $(CMAKE_BUILD) --preset compiler-off --target feedforge-demo $(PARALLEL) >>"$$log" 2>&1; then \
		cat "$$log" >&2; exit 1; \
	fi; \
	"$(BUILD_ROOT)/compiler-off/examples/feedforge-demo"

generated-check: ## Compare canonical generated headers byte-for-byte
	$(call announce,Checking committed generated headers)
	@$(CMAKE) --preset "$(GENERATE_PRESET)" $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset "$(GENERATE_PRESET)" --target check-generated $(PARALLEL)

generated-refresh: ## Rewrite canonical headers; requires CONFIRM=regenerate
	@set -eu; \
	test "$(CONFIRM)" = regenerate || { \
		printf 'Refusing to rewrite generated headers. Re-run with CONFIRM=regenerate.\n' >&2; exit 2; }; \
	if $(GIT) rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
		if test "$(FORCE)" != 1 && { \
			! $(GIT) diff --quiet -- generated/include/feedforge/generated/nasdaq/itch50_all.hpp generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp || \
			! $(GIT) diff --cached --quiet -- generated/include/feedforge/generated/nasdaq/itch50_all.hpp generated/include/feedforge/generated/nasdaq/itch50_order_events.hpp; \
		}; then \
			printf 'Generated headers are already modified; use FORCE=1 only after review.\n' >&2; exit 2; \
		fi; \
	elif test "$(FORCE)" != 1; then \
		printf 'Outside Git, generated-refresh also requires FORCE=1.\n' >&2; exit 2; \
	fi
	$(call announce,Regenerating canonical headers)
	@$(CMAKE) --preset "$(GENERATE_PRESET)" $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset "$(GENERATE_PRESET)" --target regenerate $(PARALLEL)
	@$(CMAKE_BUILD) --preset "$(GENERATE_PRESET)" --target check-generated $(PARALLEL)
	@$(GIT) diff --check -- generated/include
	@$(GIT) diff --stat -- generated/include

compiler: ## Build feedforgec in GENERATE_PRESET
	@$(CMAKE) --preset "$(GENERATE_PRESET)" $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset "$(GENERATE_PRESET)" --target feedforgec $(PARALLEL)

validate: compiler ## Validate SCHEMA and PIPELINE with feedforgec
	$(call announce,Validating $(PIPELINE))
	@"$(FEEDFORGEC)" validate --schema "$(SCHEMA)" --pipeline "$(PIPELINE)"

pipeline-compile: ## Generate PIPELINE under build/ (override GENERATED_OUTPUT)
	$(call guard_build_output,$(GENERATED_OUTPUT))
	+@$(MAKE) --no-print-directory compiler
	$(call announce,Compiling $(PIPELINE))
	@"$(FEEDFORGEC)" compile --schema "$(SCHEMA)" --pipeline "$(PIPELINE)" --output "$(GENERATED_OUTPUT)"
	@printf '%s\n' "$(GENERATED_OUTPUT)"

pipeline-ir: ## Dump canonical FFIR for PIPELINE under build/
	$(call guard_build_output,$(IR_OUTPUT))
	+@$(MAKE) --no-print-directory compiler
	$(call announce,Dumping FFIR for $(PIPELINE))
	@"$(FEEDFORGEC)" dump-ir --schema "$(SCHEMA)" --pipeline "$(PIPELINE)" \
		--output "$(IR_OUTPUT)"
	@printf '%s\n' "$(IR_OUTPUT)"

replay: ## Build replay example and run REPLAY_FILE
	@set -eu; test -n "$(REPLAY_FILE)" && test -f "$(REPLAY_FILE)" || { \
		printf 'Set REPLAY_FILE to a readable BinaryFILE path.\n' >&2; exit 2; }
	@$(CMAKE) --preset dev $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset dev --target feedforge-replay $(PARALLEL)
	@"$(BUILD_ROOT)/dev/examples/feedforge-replay" "$(REPLAY_FILE)"

replay-empty: ## Replay a valid empty complete BinaryFILE session
	@$(CMAKE) --preset dev $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset dev --target feedforge-replay $(PARALLEL)
	@printf '\000\000' > "$(BUILD_ROOT)/dev/empty-complete.binaryfile"
	@"$(BUILD_ROOT)/dev/examples/feedforge-replay" "$(BUILD_ROOT)/dev/empty-complete.binaryfile"

##@ LLVM and hardening

llvm-dev: ## Run the full suite with explicitly selected upstream LLVM
	$(require_llvm)
	$(call announce,Running full upstream LLVM suite)
	@$(CMAKE) --preset dev -B "$(LLVM_DEV_DIR)" \
		-DCMAKE_CXX_COMPILER="$(LLVM_CXX)" -DFEEDFORGE_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)
	@$(CMAKE_BUILD) "$(LLVM_DEV_DIR)" $(PARALLEL) $(BUILD_ARGS)
	@$(CTEST) --test-dir "$(LLVM_DEV_DIR)" --output-on-failure $(CTEST_ARGS)
	@$(CMAKE_BUILD) "$(LLVM_DEV_DIR)" --target check-generated $(PARALLEL)

llvm-sanitizers: ## Run ASan+UBSan with explicitly selected upstream LLVM
	$(require_llvm)
	$(call announce,Running upstream LLVM sanitizers)
	@$(CMAKE) --preset sanitizers -B "$(LLVM_SANITIZER_DIR)" \
		-DCMAKE_CXX_COMPILER="$(LLVM_CXX)" -DFEEDFORGE_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)
	@$(CMAKE_BUILD) "$(LLVM_SANITIZER_DIR)" $(PARALLEL) $(BUILD_ARGS)
	@ASAN_OPTIONS="$(ASAN_OPTIONS)" UBSAN_OPTIONS="$(UBSAN_OPTIONS)" \
		$(CTEST) --test-dir "$(LLVM_SANITIZER_DIR)" --output-on-failure $(CTEST_ARGS)

rtsan: ## Build and run the isolated RealtimeSanitizer smoke target
	$(require_llvm)
	$(call announce,Running RealtimeSanitizer smoke)
	@$(CMAKE) --preset rtsan -B "$(RTSAN_BUILD_DIR)" \
		-DCMAKE_CXX_COMPILER="$(LLVM_CXX)" -DFEEDFORGE_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)
	@$(CMAKE_BUILD) "$(RTSAN_BUILD_DIR)" --target feedforge_test_rtsan_smoke $(PARALLEL)
	@$(CTEST) --test-dir "$(RTSAN_BUILD_DIR)" --output-on-failure --no-tests=error \
		-R '^hardening\.rtsan_smoke$$'

##@ Fuzzing

fuzz-build: ## Configure and build all four libFuzzer targets
	$(require_llvm)
	$(call announce,Building libFuzzer targets)
	@$(CMAKE) --preset fuzz -B "$(FUZZ_BUILD_DIR)" \
		-DCMAKE_CXX_COMPILER="$(LLVM_CXX)" -DFEEDFORGE_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)
	@$(CMAKE_BUILD) "$(FUZZ_BUILD_DIR)" $(PARALLEL) $(BUILD_ARGS)

fuzz-binary-file: fuzz-build ## Run bounded BinaryFILE cursor fuzzing
	$(call run_fuzzer,binary_file)

fuzz-decode-one: fuzz-build ## Run bounded all-message decode fuzzing
	$(call run_fuzzer,decode_one)

fuzz-differential-decode: fuzz-build ## Run bounded independent differential decode fuzzing
	$(call run_fuzzer,differential_decode)

fuzz-replay: fuzz-build ## Run bounded strict replay fuzzing
	$(call run_fuzzer,replay)

fuzz-smoke: fuzz-build ## Run all four bounded fuzz targets sequentially
	$(call run_fuzzer,binary_file)
	$(call run_fuzzer,decode_one)
	$(call run_fuzzer,differential_decode)
	$(call run_fuzzer,replay)

##@ Benchmarks

bench-smoke: ## Build and run the non-comparison benchmark smoke test
	+@$(MAKE) --no-print-directory check PRESET=bench

bench-run: ## Collect a frozen series; requires BENCH_LABEL and BENCH_SOURCE_ID
	@set -eu; \
	case "$(BENCH_LABEL)" in ''|*[!A-Za-z0-9._-]*) \
		printf 'BENCH_LABEL must match [A-Za-z0-9._-]+\n' >&2; exit 2 ;; esac; \
	test -n "$(BENCH_SOURCE_ID)" || { printf 'Set BENCH_SOURCE_ID to an immutable revision.\n' >&2; exit 2; }; \
	if test -n "$${CI:-}" && test "$(FORCE)" != 1; then \
		printf 'Refusing timing under CI; use FORCE=1 only for an intentional run.\n' >&2; exit 2; \
	fi; \
	if test -d "$(BENCH_OUTPUT_DIR)" && test -n "$$(find "$(BENCH_OUTPUT_DIR)" -mindepth 1 -print -quit)"; then \
		printf 'Benchmark output already exists: %s\n' "$(BENCH_OUTPUT_DIR)" >&2; exit 2; \
	fi; \
	if test "$(UNAME_S)" = Darwin; then \
		printf 'Note: macOS has no supported process-affinity control; preserve this caveat.\n'; \
	fi
	+@$(MAKE) --no-print-directory bench-smoke
	@$(PYTHON) benchmarks/benchmark.py run \
		--executable "$(BENCH_EXECUTABLE)" \
		--output-dir "$(BENCH_OUTPUT_DIR)" \
		--label "$(BENCH_LABEL)" \
		--source-id "$(BENCH_SOURCE_ID)" \
		--correctness-command "ctest --preset bench" \
		--cwd "$(ROOT)"

bench-compare: ## Compare explicit BENCH_BASELINE and BENCH_CANDIDATE series
	@set -eu; \
	test -f "$(BENCH_BASELINE)" || { printf 'Set BENCH_BASELINE to series.json.\n' >&2; exit 2; }; \
	test -f "$(BENCH_CANDIDATE)" || { printf 'Set BENCH_CANDIDATE to series.json.\n' >&2; exit 2; }; \
	test -n "$(strip $(BENCH_TARGETS))" || { \
		printf 'Set BENCH_TARGETS to the predeclared benchmark IDs.\n' >&2; exit 2; }; \
	if test "$(FORCE)" != 1 && { test -e "$(BENCH_COMPARE_JSON)" || test -e "$(BENCH_COMPARE_CSV)"; }; then \
		printf 'Comparison output exists; choose new paths or set FORCE=1.\n' >&2; exit 2; \
	fi; \
	$(CMAKE) -E make_directory "$$(dirname "$(BENCH_COMPARE_JSON)")" "$$(dirname "$(BENCH_COMPARE_CSV)")"
	@$(PYTHON) benchmarks/benchmark.py compare \
		--baseline "$(BENCH_BASELINE)" \
		--candidate "$(BENCH_CANDIDATE)" \
		$(BENCH_TARGET_ARGS) \
		--output-json "$(BENCH_COMPARE_JSON)" \
		--output-csv "$(BENCH_COMPARE_CSV)"

##@ Packaging

release-assets: ## Build deterministic source archives and SHA256SUMS
	$(call announce,Building release assets from $(RELEASE_REVISION))
	@$(PYTHON) tools/release_assets.py build \
		--repository "$(ROOT)" \
		--revision "$(RELEASE_REVISION)" \
		--output-dir "$(RELEASE_OUTPUT_DIR)"

release-assets-check: ## Test release tooling and require byte-identical rebuilds
	$(call announce,Checking deterministic release assets)
	@$(PYTHON) tools/release_assets_test.py
	@$(PYTHON) tools/release_assets.py check \
		--repository "$(ROOT)" \
		--revision "$(RELEASE_REVISION)"

install: ## Build INSTALL_PRESET and install to PREFIX
	$(call announce,Installing $(INSTALL_PRESET) to $(PREFIX))
	@$(CMAKE) --preset "$(INSTALL_PRESET)" $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset "$(INSTALL_PRESET)" $(PARALLEL) $(BUILD_ARGS)
	@$(CMAKE) --install "$(BUILD_ROOT)/$(INSTALL_PRESET)" --prefix "$(PREFIX)"

install-runtime: ## Install compiler-off C++20 runtime to RUNTIME_PREFIX
	$(call announce,Installing runtime-only package to $(RUNTIME_PREFIX))
	@$(CMAKE) --preset compiler-off $(CMAKE_ARGS)
	@$(CMAKE_BUILD) --preset compiler-off $(PARALLEL) $(BUILD_ARGS)
	@$(CMAKE) --install "$(BUILD_ROOT)/compiler-off" --prefix "$(RUNTIME_PREFIX)"

##@ Quality and portability

lint: ## Validate whitespace and shared CMake preset syntax
	$(call announce,Running lightweight repository checks)
	@$(GIT) diff --check
	@$(GIT) diff --cached --check
	@$(CMAKE) --list-presets=all >/dev/null

format-check: ## Check formatting of changed non-generated C++ lines
	@command -v "$(GIT_CLANG_FORMAT)" >/dev/null 2>&1 || { printf 'git-clang-format not found.\n' >&2; exit 2; }
	@set -eu; \
	tmp="$$(mktemp "$${TMPDIR:-/tmp}/feedforge-format.XXXXXX")"; \
	trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	rc=0; "$(GIT_CLANG_FORMAT)" --binary "$(CLANG_FORMAT)" --extensions "$(FORMAT_EXTENSIONS)" --diff HEAD -- $(FORMAT_PATHS) $(FORMAT_EXCLUDES) > "$$tmp" 2>&1 || rc=$$?; \
	if grep -q '^diff --git ' "$$tmp"; then cat "$$tmp"; exit 1; fi; \
	cat "$$tmp"; \
	if test "$$rc" -ne 0; then exit "$$rc"; fi

format-changed: ## Format changed non-generated C++ lines; requires CONFIRM=format
	@set -eu; test "$(CONFIRM)" = format || { \
		printf 'Refusing to rewrite source. Re-run with CONFIRM=format.\n' >&2; exit 2; }
	@set -eu; \
	tmp="$$(mktemp "$${TMPDIR:-/tmp}/feedforge-format.XXXXXX")"; \
	trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	rc=0; "$(GIT_CLANG_FORMAT)" --binary "$(CLANG_FORMAT)" --extensions "$(FORMAT_EXTENSIONS)" --force HEAD -- $(FORMAT_PATHS) $(FORMAT_EXCLUDES) > "$$tmp" 2>&1 || rc=$$?; \
	cat "$$tmp"; \
	if test "$$rc" -ne 0 && ! grep -q '^changed files:' "$$tmp"; then exit "$$rc"; fi

tidy: ## Run upstream LLVM clang-tidy without modifying files
	$(require_llvm)
	@command -v "$(RUN_CLANG_TIDY)" >/dev/null 2>&1 || { printf 'run-clang-tidy not found.\n' >&2; exit 2; }
	@$(CMAKE) --preset dev -B "$(LLVM_DEV_DIR)" \
		-DCMAKE_CXX_COMPILER="$(LLVM_CXX)" -DFEEDFORGE_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)
	@$(CMAKE_BUILD) "$(LLVM_DEV_DIR)" $(PARALLEL) $(BUILD_ARGS)
	@$(CMAKE) -E make_directory "$$(dirname "$(TIDY_LOG)")"
	@set -eu; \
	rc=0; \
	"$(RUN_CLANG_TIDY)" -p "$(LLVM_DEV_DIR)" -clang-tidy-binary "$(CLANG_TIDY)" \
		-header-filter='^$(ROOT)/(include|src|tests|fuzz|benchmarks|examples)/' \
		-j "$(TIDY_JOBS)" -quiet -hide-progress > "$(TIDY_LOG)" 2>&1 || rc=$$?; \
	warnings="$$(grep -c ': warning:' "$(TIDY_LOG)" || true)"; \
	errors="$$(grep -c ': error:' "$(TIDY_LOG)" || true)"; \
	printf 'clang-tidy: %s warning(s), %s error(s)\n' "$$warnings" "$$errors"; \
	printf 'Full diagnostics: %s\n' "$(TIDY_LOG)"; \
	if test "$$rc" -ne 0; then tail -n 80 "$(TIDY_LOG)"; exit "$$rc"; fi

linux-smoke: ## Run indexed sources read-only through native Linux Docker
	@command -v "$(DOCKER)" >/dev/null 2>&1 || { printf 'Docker not found.\n' >&2; exit 2; }
	$(call announce,Running $(LINUX_IMAGE) smoke on $(DOCKER_PLATFORM))
	@set -eu; \
	list="$$(mktemp "$${TMPDIR:-/tmp}/feedforge-linux-list.XXXXXX")"; \
	archive="$$(mktemp "$${TMPDIR:-/tmp}/feedforge-linux-src.XXXXXX")"; \
	volume="feedforge-linux-$$(date +%s)-$$$$"; \
	http_proxy="$$( $(DOCKER) info --format '{{.HTTPProxy}}' 2>/dev/null || true )"; \
	https_proxy="$$( $(DOCKER) info --format '{{.HTTPSProxy}}' 2>/dev/null || true )"; \
	no_proxy="$$( $(DOCKER) info --format '{{.NoProxy}}' 2>/dev/null || true )"; \
	cleanup() { \
		$(DOCKER) volume rm -f "$$volume" >/dev/null 2>&1 || true; \
		rm -f "$$list" "$$archive"; \
	}; \
	trap cleanup EXIT HUP INT TERM; \
	$(GIT) ls-files --cached -z > "$$list"; \
	COPYFILE_DISABLE=1 tar --no-xattrs --null -T "$$list" -cf "$$archive"; \
	$(DOCKER) volume create "$$volume" >/dev/null; \
	$(DOCKER) run --rm -i --platform "$(DOCKER_PLATFORM)" \
		--mount "type=volume,src=$$volume,dst=/src" \
		-w /src "$(LINUX_IMAGE)" /bin/sh -c \
		'tar -xf - && mkdir -p build' < "$$archive"; \
	$(DOCKER) run --rm --platform "$(DOCKER_PLATFORM)" \
		--mount "type=volume,src=$$volume,dst=/src,readonly" \
		--tmpfs /src/build:exec,mode=1777 \
		--env "HTTP_PROXY=$$http_proxy" --env "HTTPS_PROXY=$$https_proxy" \
		--env "NO_PROXY=$$no_proxy" \
		--env "http_proxy=$$http_proxy" --env "https_proxy=$$https_proxy" \
		--env "no_proxy=$$no_proxy" \
		-w /src "$(LINUX_IMAGE)" /bin/sh -c \
		'apt-get update >/dev/null && \
		 DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates cmake g++ git ninja-build >/dev/null && \
		 cmake --preset fast -DFEEDFORGE_WARNINGS_AS_ERRORS=ON && \
		 cmake --build --preset fast --parallel "$(LINUX_JOBS)" && \
		 ctest --preset fast'

##@ Cleanup

clean: ## Run the configured PRESET clean target without removing its cache
	@case "$(PRESET)" in ''|*[!A-Za-z0-9._-]*) printf 'Unsafe PRESET value\n' >&2; exit 2 ;; esac
	@$(CMAKE_BUILD) --preset "$(PRESET)" --target clean

clobber: ## Remove ignored build/ and out/ trees; requires CONFIRM=clobber
	@set -eu; \
	test "$(CONFIRM)" = clobber || { \
		printf 'Refusing to remove build outputs. Re-run with CONFIRM=clobber.\n' >&2; exit 2; }; \
	top="$$( $(GIT) rev-parse --show-toplevel 2>/dev/null )"; \
	test "$$top" = "$(ROOT)" || { \
		printf 'Refusing to remove outputs outside the FeedForge Git root.\n' >&2; exit 2; }; \
	test ! -L "$(BUILD_ROOT)" && test ! -L "$(OUT_ROOT)" || { \
		printf 'Refusing to remove symlinked output roots.\n' >&2; exit 2; }; \
	for protected in \
		"$(BUILD_ROOT)/bench/private-holdout" \
		"$(BUILD_ROOT)/bench/results"; do \
		if test -e "$$protected" || test -L "$$protected"; then \
			printf 'Refusing to remove protected benchmark data: %s\n' "$$protected" >&2; \
			printf 'Relocate it deliberately before using clobber.\n' >&2; exit 2; \
		fi; \
	done; \
	rm -rf "$(BUILD_ROOT)" "$(OUT_ROOT)"
