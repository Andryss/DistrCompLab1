CC = clang
CFLAGS = -std=c99 -Wall -pedantic
SANITIZE ?= 0

ifeq ($(SANITIZE),1)
BUILD_DIR ?= build/sanitizers
CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=address,undefined
else
BUILD_DIR ?= build
endif

SOURCE_DIRS := src src/process src/ipc src/utils src/provided
CPPFLAGS = $(addprefix -I,$(SOURCE_DIRS))
SOURCES := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
HEADERS := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.h))
LIB_SOURCES := $(filter-out src/main.c,$(SOURCES))
TEST_SOURCES := $(wildcard tests/*.c)
TEST_HEADERS := $(wildcard tests/*.h)
TEST_CPPFLAGS = $(shell pkg-config --cflags cmocka) \
    -DPA1_REPOSITORY='"$(CURDIR)"' \
    -DPA1_DEFAULT_BINARY='"$(abspath $(BUILD_DIR))/pa1"' \
    -DPA1_BUILD_DIRECTORY='"$(abspath $(BUILD_DIR))"'

.PHONY: all tests test package clean
all: $(BUILD_DIR)/pa1

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/pa1: $(SOURCES) $(HEADERS) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SOURCES) $(LDLIBS) -o "$@"

$(BUILD_DIR)/pa1_tests: $(TEST_SOURCES) $(TEST_HEADERS) $(LIB_SOURCES) $(HEADERS) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(TEST_SOURCES) $(LIB_SOURCES) $(shell pkg-config --libs cmocka) $(LDLIBS) -o "$@"

tests: $(BUILD_DIR)/pa1 $(BUILD_DIR)/pa1_tests

test: tests
	"$(BUILD_DIR)/pa1_tests"

package:
	rm -rf "$(BUILD_DIR)/dist/pa1"
	mkdir -p "$(BUILD_DIR)/dist/pa1"
	cp $(SOURCES) $(HEADERS) "$(BUILD_DIR)/dist/pa1/"
	COPYFILE_DISABLE=1 tar -czf "$(BUILD_DIR)/dist/pa1.tar.gz" -C "$(BUILD_DIR)/dist" pa1

clean:
	rm -rf "$(BUILD_DIR)"
