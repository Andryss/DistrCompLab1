CC = clang
CLANG_TIDY ?= clang-tidy
CFLAGS = -std=c99 -Wall -pedantic
SANITIZER_FLAGS = -fsanitize=address,undefined \
    -fno-sanitize-recover=all -fno-omit-frame-pointer -g
BUILD_DIR ?= build
TEST_BUILD_DIR = $(BUILD_DIR)/sanitizers

SOURCE_DIRS := src src/process src/ipc src/utils src/provided
CPPFLAGS = $(addprefix -I,$(SOURCE_DIRS))
SOURCES := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
HEADERS := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.h))
LIB_SOURCES := $(filter-out src/main.c,$(SOURCES))
TEST_SOURCES := $(wildcard tests/*.c)
TEST_HEADERS := $(wildcard tests/*.h)
TEST_CPPFLAGS = $(shell pkg-config --cflags cmocka) \
    -DPA1_REPOSITORY='"$(CURDIR)"' \
    -DPA1_DEFAULT_BINARY='"$(abspath $(TEST_BUILD_DIR))/pa1"' \
    -DPA1_BUILD_DIRECTORY='"$(abspath $(TEST_BUILD_DIR))"'

.PHONY: all tests test lint package clean
all: $(BUILD_DIR)/pa1

$(TEST_BUILD_DIR)/pa1 $(TEST_BUILD_DIR)/pa1_tests: CFLAGS += $(SANITIZER_FLAGS)
$(TEST_BUILD_DIR)/pa1_tests: CPPFLAGS += $(TEST_CPPFLAGS)
$(TEST_BUILD_DIR)/pa1_tests: LDLIBS += $(shell pkg-config --libs cmocka)

$(BUILD_DIR)/pa1 $(TEST_BUILD_DIR)/pa1: $(SOURCES)
$(TEST_BUILD_DIR)/pa1_tests: $(TEST_SOURCES) $(LIB_SOURCES) $(TEST_HEADERS)

# LINK.c содержит CC, CFLAGS, CPPFLAGS и LDFLAGS; в компилятор передаём только .c.
$(BUILD_DIR)/pa1 $(TEST_BUILD_DIR)/pa1 $(TEST_BUILD_DIR)/pa1_tests: $(HEADERS) Makefile
	mkdir -p "$(@D)"
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o "$@"

tests: $(TEST_BUILD_DIR)/pa1 $(TEST_BUILD_DIR)/pa1_tests

test: tests
	"$(TEST_BUILD_DIR)/pa1_tests"

lint:
	$(CLANG_TIDY) $(SOURCES) -- $(CPPFLAGS) $(CFLAGS)
	$(CLANG_TIDY) $(TEST_SOURCES) -- $(CPPFLAGS) $(TEST_CPPFLAGS) $(CFLAGS)

package:
	rm -rf "$(BUILD_DIR)/dist/pa1"
	mkdir -p "$(BUILD_DIR)/dist/pa1"
	cp $(SOURCES) $(HEADERS) "$(BUILD_DIR)/dist/pa1/"
	COPYFILE_DISABLE=1 tar -czf "$(BUILD_DIR)/dist/pa1.tar.gz" -C "$(BUILD_DIR)/dist" pa1

clean:
	rm -rf "$(BUILD_DIR)"
