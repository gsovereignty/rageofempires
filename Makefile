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
ifeq ($(shell uname -s),Darwin)
PLATFORM_CMAKE_ARGS := -DAOE_BUILD_SDL3=ON
endif

.DEFAULT_GOAL := build

.PHONY: all configure build run web test clean help

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

web:
	./web/bootstrap_emsdk.sh
	. "build-web/emsdk/emsdk_env.sh" && \
		emcmake $(CMAKE) -S . -B build-web \
			-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
			-DAOE_BUILD_WEB=ON \
			-DAOE_ENABLE_MPG123=OFF && \
		$(CMAKE) --build build-web --target aoe_web \
			--parallel "$(JOBS)" && \
		$(PYTHON) -m http.server "$(WEB_PORT)" \
			--directory build-web/dist

test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" --parallel "$(JOBS)" \
		--output-on-failure

clean:
	$(CMAKE) -E remove_directory "$(BUILD_DIR)"

help:
	@echo "make                 Configure and build"
	@echo "make run             Build and launch the game"
	@echo "make web             Build web game and serve it on http://localhost:$(WEB_PORT)"
	@echo "make test            Build and run all tests"
	@echo "make clean           Clean compiled outputs"
	@echo "Variables: BUILD_DIR, WEB_PORT, BUILD_TYPE, JOBS, CMAKE, CTEST, PYTHON, CMAKE_ARGS"
