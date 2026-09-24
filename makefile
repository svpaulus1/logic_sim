.PHONY: all config build run r test t release doxygen d clean c

# Build type. Override on the command line: make run TYPE=Release
#   Debug          -g            no optimization, asserts on, sanitizers on
#   Release        -O3 -DNDEBUG  fastest, asserts compiled out
#   RelWithDebInfo -O2 -g        optimized but debuggable
#   MinSizeRel     -Os           smallest binary
TYPE      ?= Debug
BUILD_DIR := build/$(TYPE)
BIN       := $(BUILD_DIR)/logicsim

# API reference generated from the comments in include/ and src/.
DOXY_DIR  := build/doxygen
DOXY_PDF  := $(DOXY_DIR)/logicsim.pdf

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

# PDF of every Doxygen comment (settings in Doxyfile). Needs doxygen and
# pdflatex; Ghostscript too for the class diagrams (skipped without it).
# LaTeX's chatter goes to $(DOXY_DIR)/latex.log.
doxygen:
	@command -v doxygen >/dev/null || { echo "doxygen not found (Fedora: sudo dnf install doxygen doxygen-latex)"; exit 1; }
	@command -v pdflatex >/dev/null || { echo "pdflatex not found (Fedora: sudo dnf install doxygen-latex)"; exit 1; }
	rm -rf $(DOXY_DIR)
	@if command -v gs >/dev/null; then doxygen Doxyfile; else \
		echo "Ghostscript (gs) not found: leaving out class diagrams (Fedora: sudo dnf install ghostscript)"; \
		{ cat Doxyfile; echo "CLASS_GRAPH = NO"; } | doxygen -; fi
	@echo "Running LaTeX (log: $(DOXY_DIR)/latex.log)"
	@$(MAKE) -C $(DOXY_DIR)/latex > $(DOXY_DIR)/latex.log 2>&1 || \
		{ tail -n 30 $(DOXY_DIR)/latex.log; echo "LaTeX failed, see $(DOXY_DIR)/latex/refman.log"; exit 1; }
	cp $(DOXY_DIR)/latex/refman.pdf $(DOXY_PDF)
	@echo "PDF written to $(DOXY_PDF)"

d: doxygen

clean:
	rm -rf build

c: clean
