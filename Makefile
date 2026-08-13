CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= $(shell \
	nproc 2>/dev/null || \
	sysctl -n hw.logicalcpu 2>/dev/null || \
	getconf _NPROCESSORS_ONLN 2>/dev/null || \
	echo 1)
CMAKE_ARGS ?=
WEB_PORT ?= 8888
PYTHON ?= python3
NOSTR_AUDIT_PYTHON ?= build-web/selenium-venv/bin/python
NOSTR_AUDIT_RELAYS ?=
NOSTR_AUDIT_PORT ?= 8892
NOSTR_AUDIT_ARGS ?=
ifeq ($(shell uname -s),Darwin)
PLATFORM_CMAKE_ARGS := -DAOE_BUILD_SDL3=ON
endif

.DEFAULT_GOAL := build

.PHONY: all configure build run web web-build web-tests \
	web-tests-only audit-browser-risk-spike audit-browser-risk-spike-only \
	audit-nostr-oracles audit-nostr-multiplayer \
	ci-nostr-visual-per-change ci-nostr-visual-display-matrix \
	ci-nostr-visual-seeds \
	ci-nostr-visual-scheduled \
	check-all clean help

all: build

configure:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		cached_source="$$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$(BUILD_DIR)/CMakeCache.txt")"; \
		current_source="$$(pwd -P)"; \
		if [ "$$cached_source" != "$$current_source" ]; then \
			echo "Source tree moved; resetting $(BUILD_DIR)"; \
			$(CMAKE) -E remove_directory "$(BUILD_DIR)"; \
		fi; \
	fi
	$(CMAKE) -S . -B "$(BUILD_DIR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" $(PLATFORM_CMAKE_ARGS) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"

run: build
	@if [ "$$(uname -s)" = Darwin ] && \
		[ -d "$(BUILD_DIR)/AoE Archaeology.app" ]; then \
		"$(BUILD_DIR)/AoE Archaeology.app/Contents/MacOS/AoE Archaeology"; \
	else \
		"$(BUILD_DIR)/aoe_reconstruction"; \
	fi

web-build:
	./web/bootstrap_emsdk.sh
	. "build-web/emsdk/emsdk_env.sh" && \
		emcmake $(CMAKE) -S . -B build-web \
			-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
			-DAOE_BUILD_WEB=ON \
			-DAOE_ENABLE_MPG123=OFF && \
		$(CMAKE) --build build-web --target aoe_web \
			--parallel "$(JOBS)"

web: web-build
	$(PYTHON) -m http.server "$(WEB_PORT)" \
			--directory build-web/dist

web-tests:
	$(MAKE) web-build
	$(MAKE) web-tests-only

web-tests-only:
	npm --prefix web/nostr test
	npm --prefix web/nostr run typecheck
	@test -x "$(NOSTR_AUDIT_PYTHON)" || { \
		echo "Missing isolated Selenium Python: $(NOSTR_AUDIT_PYTHON)" >&2; \
		exit 1; \
	}
	"$(NOSTR_AUDIT_PYTHON)" tests/web/test_nostr_multiplayer_audit_tools.py

audit-browser-risk-spike:
	$(MAKE) web-build
	$(MAKE) audit-browser-risk-spike-only

audit-browser-risk-spike-only:
	@test -x "$(NOSTR_AUDIT_PYTHON)" || { \
		echo "Missing isolated Selenium Python: $(NOSTR_AUDIT_PYTHON)" >&2; \
		exit 1; \
	}
	"$(NOSTR_AUDIT_PYTHON)" tests/web/browser_risk_spike_test.py \
		--browser chrome
	"$(NOSTR_AUDIT_PYTHON)" tests/web/browser_risk_spike_test.py \
		--browser chrome --display-matrix
	"$(NOSTR_AUDIT_PYTHON)" tests/web/browser_risk_spike_test.py \
		--browser chrome --persistence-checks

test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" --parallel "$(JOBS)" \
		--output-on-failure

audit-nostr-multiplayer: web-build
	@test -x "$(NOSTR_AUDIT_PYTHON)" || { \
		echo "Missing isolated Selenium Python: $(NOSTR_AUDIT_PYTHON)" >&2; \
		exit 1; \
	}
	@relay_args=""; \
		if [ -n "$(NOSTR_AUDIT_RELAYS)" ]; then \
			relay_args="--relays $(NOSTR_AUDIT_RELAYS)"; \
		fi; \
		"$(NOSTR_AUDIT_PYTHON)" tools/run_nostr_visual_audit.py \
			--port "$(NOSTR_AUDIT_PORT)" \
			$$relay_args $(NOSTR_AUDIT_ARGS)

audit-nostr-oracles:
	@test -x "$(NOSTR_AUDIT_PYTHON)" || { \
		echo "Missing isolated Selenium Python: $(NOSTR_AUDIT_PYTHON)" >&2; \
		exit 1; \
	}
	@for test_file in \
		tools/test_nostr_visual_frame_oracle.py \
		tools/test_nostr_slp_decoder.py \
		tools/test_nostr_packaged_pixel_oracle.py \
		tools/test_nostr_visual_pixel_oracle.py \
		tools/test_nostr_visual_coverage.py \
		tools/test_nostr_visual_transition_oracle.py \
		tools/test_nostr_seeded_action_generator.py \
		tools/test_audit_multiplayer_screenshots.py \
		tools/test_run_nostr_visual_audit.py \
		tools/test_run_nostr_visual_display_matrix.py \
		tests/web/test_nostr_multiplayer_audit_tools.py; do \
		"$(NOSTR_AUDIT_PYTHON)" "$$test_file" || exit $$?; \
	done

ci-nostr-visual-per-change: build web-tests audit-nostr-oracles

ci-nostr-visual-display-matrix: web-build
	@"$(NOSTR_AUDIT_PYTHON)" tools/run_nostr_visual_display_matrix.py \
		--port "$(NOSTR_AUDIT_PORT)" \
		$(if $(NOSTR_AUDIT_RELAYS),--relays "$(NOSTR_AUDIT_RELAYS)",) \
		$(NOSTR_AUDIT_ARGS)

ci-nostr-visual-seeds: web-build
	@rotating_seed="$$($(NOSTR_AUDIT_PYTHON) \
		tools/nostr_seeded_action_generator.py \
		--commit "$$(git rev-parse HEAD)")"; \
	for seed in 11055785183250 11055785183251 11055785183252 \
		"$$rotating_seed"; do \
		"$(NOSTR_AUDIT_PYTHON)" tools/run_nostr_visual_audit.py \
			--port "$(NOSTR_AUDIT_PORT)" --seed "$$seed" \
			$(if $(NOSTR_AUDIT_RELAYS),--relays "$(NOSTR_AUDIT_RELAYS)",) \
			$(NOSTR_AUDIT_ARGS) || exit $$?; \
	done

ci-nostr-visual-scheduled: ci-nostr-visual-per-change \
	ci-nostr-visual-display-matrix ci-nostr-visual-seeds

check-all:
	$(PYTHON) scripts/run_check_all.py --make "$(MAKE)" \
		$(if $(filter 1,$(PROBLEMS_ONLY)),--problems-only,)

clean:
	$(CMAKE) -E remove_directory "$(BUILD_DIR)"

help:
	@echo "make                 Configure and build"
	@echo "make run             Build and launch the game"
	@echo "make web-build       Build packaged web game"
	@echo "make web             Build web game and serve it on http://localhost:$(WEB_PORT)"
	@echo "make test            Build and run all tests"
	@echo "make web-tests       Build web package and run browser-runtime tests"
	@echo "make audit-browser-risk-spike  Run packaged browser acceptance matrix"
	@echo "make audit-nostr-multiplayer  Run packaged two-browser public-relay audit"
	@echo "make audit-nostr-oracles      Run independent visual oracle tests"
	@echo "make ci-nostr-visual-per-change  Run required build and oracle tier"
	@echo "make ci-nostr-visual-display-matrix  Run aspect-ratio and DPR matrix"
	@echo "make ci-nostr-visual-seeds    Run fixed and source-derived seeds"
	@echo "make ci-nostr-visual-scheduled   Run full public-relay scheduled tier"
	@echo "make check-all       Run all native/web tests and browser audits"
	@echo "make check-all PROBLEMS_ONLY=1  Print only failing-stage diagnostics"
	@echo "make clean           Clean compiled outputs"
	@echo "Variables: BUILD_DIR, WEB_PORT, BUILD_TYPE, JOBS, CMAKE, CTEST, PYTHON, CMAKE_ARGS"
	@echo "Audit variables: NOSTR_AUDIT_PYTHON, NOSTR_AUDIT_RELAYS, NOSTR_AUDIT_PORT, NOSTR_AUDIT_ARGS, PROBLEMS_ONLY"
