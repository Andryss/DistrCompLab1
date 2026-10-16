CC = clang
CLANG_TIDY ?= clang-tidy
DOCKER ?= podman

APP_NAME ?= pa6
DOCKER_IMAGE ?= $(APP_NAME):local
DOCKER_ARGS ?= -p 5 --mutexl
BUILD_DIR ?= build
TEST_BUILD_DIR := $(BUILD_DIR)/sanitizers
DIST_DIR := $(BUILD_DIR)/dist
PACKAGE_DIR := $(DIST_DIR)/$(APP_NAME)
PACKAGE_ARCHIVE := $(DIST_DIR)/$(APP_NAME).tar.gz

RUNTIME_ARCHIVE := docs/PAs_master/pa2345_starter_code.tar.gz
RUNTIME_DIRECTORY := pa2345_starter_code/lib64
RUNTIME_DIR := $(BUILD_DIR)/runtime/$(RUNTIME_DIRECTORY)
RUNTIME_LIB ?= $(RUNTIME_DIR)/libruntime.so

APP_BINARY := $(BUILD_DIR)/$(APP_NAME)
MOCK_APP_BINARY := $(BUILD_DIR)/mock/$(APP_NAME)
TEST_APP_BINARY := $(TEST_BUILD_DIR)/$(APP_NAME)
TEST_BINARY := $(TEST_BUILD_DIR)/$(APP_NAME)_tests
TEST_RUNTIME := tests/util/runtime.c

CFLAGS = -std=c99 -Wall -pedantic
SANITIZER_FLAGS := -fsanitize=address,undefined \
    -fno-sanitize-recover=all -fno-omit-frame-pointer -g

# clang-tidy из IDE может не находить заголовки компилятора и macOS SDK.
# Эти пути нужны только анализатору; обычный clang определяет их самостоятельно.
TIDY_FLAGS := -resource-dir=$(shell $(CC) -print-resource-dir)
ifeq ($(shell uname -s),Darwin)
TIDY_FLAGS += -isysroot $(shell xcrun --show-sdk-path)
endif

SOURCE_DIRS := src src/process src/ipc src/utils src/provided
TEST_DIRS := tests tests/util
CPPFLAGS = $(addprefix -I,$(SOURCE_DIRS))
SOURCES := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
HEADERS := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.h))
LIB_SOURCES := $(filter-out src/main.c,$(SOURCES))
TEST_SOURCES := $(foreach dir,$(TEST_DIRS),$(wildcard $(dir)/*.c))
TEST_HEADERS := $(foreach dir,$(TEST_DIRS),$(wildcard $(dir)/*.h))
TEST_CPPFLAGS = $(addprefix -I,$(TEST_DIRS)) $(shell pkg-config --cflags cmocka) \
    -Dwrite=test_write \
    -DTEST_APP_NAME='"$(APP_NAME)"' \
    -DTEST_REPOSITORY='"$(CURDIR)"' \
    -DTEST_DEFAULT_BINARY='"$(abspath $(TEST_APP_BINARY))"' \
    -DTEST_BUILD_DIRECTORY='"$(abspath $(TEST_BUILD_DIR))"' \
    -DTEST_RUNTIME_SOURCE='"$(abspath $(TEST_RUNTIME))"' \
    -DTEST_RUNTIME_LIBRARY='"$(abspath $(TEST_BUILD_DIR))/runtime/$(RUNTIME_DIRECTORY)/libruntime.so"'

.PHONY: all mock docker tests test test-linux lint package runtime clean
all: $(APP_BINARY)

# Локальное приложение с заменой print(), без Linux-библиотеки и cmocka.
mock: $(MOCK_APP_BINARY)

# Оригинальная библиотека рассчитана на Linux x86-64, включая запуск с Apple Silicon.
docker:
	$(DOCKER) build --platform linux/amd64 --build-arg APP_NAME="$(APP_NAME)" -t "$(DOCKER_IMAGE)" .
	$(DOCKER) run --rm --init --platform linux/amd64 "$(DOCKER_IMAGE)" $(DOCKER_ARGS)

runtime: $(RUNTIME_LIB)

$(RUNTIME_DIR)/libruntime.so: $(RUNTIME_ARCHIVE)
	mkdir -p "$(BUILD_DIR)/runtime"
	tar -xzf "$(RUNTIME_ARCHIVE)" -C "$(BUILD_DIR)/runtime" "$(RUNTIME_DIRECTORY)/libruntime.so"
	touch "$@"

$(APP_BINARY): $(SOURCES) $(RUNTIME_LIB) | runtime
$(APP_BINARY): LDLIBS += -L"$(dir $(RUNTIME_LIB))" -lruntime

$(TEST_APP_BINARY) $(TEST_BINARY): CFLAGS += $(SANITIZER_FLAGS)
$(TEST_APP_BINARY): $(SOURCES) $(TEST_RUNTIME)
$(MOCK_APP_BINARY): $(SOURCES) $(TEST_RUNTIME)
$(TEST_BINARY): $(TEST_SOURCES) $(LIB_SOURCES) $(TEST_HEADERS)
$(TEST_BINARY): CPPFLAGS += $(TEST_CPPFLAGS)
$(TEST_BINARY): LDLIBS += $(shell pkg-config --libs cmocka)

# В компилятор передаются только исходники, без заголовков и служебных целей.
$(APP_BINARY) $(MOCK_APP_BINARY) $(TEST_APP_BINARY) $(TEST_BINARY): $(HEADERS) Makefile
	mkdir -p "$(@D)"
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o "$@"

tests: $(TEST_APP_BINARY) $(TEST_BINARY)

test: tests
	"$(TEST_BINARY)"

# Тот же C/cmocka runner собирает архив с оригинальной Linux-библиотекой.
test-linux: tests
	"$(TEST_BINARY)" --linux

lint:
	$(CLANG_TIDY) $(SOURCES) -- $(CPPFLAGS) $(CFLAGS) $(TIDY_FLAGS)
	$(CLANG_TIDY) $(TEST_SOURCES) -- $(CPPFLAGS) $(TEST_CPPFLAGS) $(CFLAGS) $(TIDY_FLAGS)

package: runtime
	rm -rf "$(PACKAGE_DIR)"
	mkdir -p "$(PACKAGE_DIR)"
	cp $(SOURCES) $(HEADERS) "$(PACKAGE_DIR)/"
	cp "$(RUNTIME_LIB)" "$(PACKAGE_DIR)/libruntime.so"
	COPYFILE_DISABLE=1 tar -czf "$(PACKAGE_ARCHIVE)" -C "$(DIST_DIR)" "$(APP_NAME)"

clean:
	rm -rf "$(BUILD_DIR)"
