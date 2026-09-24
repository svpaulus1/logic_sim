.PHONY: all config build run r test t release clean c

# Build type. Override on the command line: make run TYPE=Release
#   Debug          -g            no optimization, asserts on, sanitizers on
#   Release        -O3 -DNDEBUG  fastest, asserts compiled out
#   RelWithDebInfo -O2 -g        optimized but debuggable
#   MinSizeRel     -Os           smallest binary
TYPE      ?= Debug
BUILD_DIR := build/$(TYPE)
BIN       := $(BUILD_DIR)/logicsim

all: build

config:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(TYPE)

$(BUILD_DIR)/CMakeCache.txt:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(TYPE)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR)

run: build
	./$(BIN)

r: run

# Unit tests (Debug by default, so asserts and sanitizers are on).
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

t: test

release:
	$(MAKE) run TYPE=Release

clean:
	rm -rf build

c: clean
