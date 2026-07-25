CC := cc
CFLAGS := -std=gnu2x
TARGET := tatr
BUILD ?= Debug

# Options (set to 1 to enable)
WARNINGS ?= 1
ASAN ?= 0
UBSAN ?= 0
MODULE_EXPORT ?= 1

MAIN_DIR := src/main
CORE_DIR := src/core
COMMANDS_DIR := src/commands
MODULES_DIR := src/modules
THIRDPARTY_DIR := src/thirdparty
BUILD_DIR := build
API_DIR := src/api
STORAGE_DIR := src/storage
RENDER_DIR := src/render

# Sources
read_sources = $(shell sed -e 's/#.*//' -e '/^[[:space:]]*$$/d' sources/$(1).txt)

MAIN_SRC       := $(call read_sources,main)
CORE_SRC       := $(call read_sources,core)
API_SRC        := $(call read_sources,api)
STORAGE_SRC    := $(call read_sources,storage)
RENDER_SRC     := $(call read_sources,render)
COMMANDS_SRC   := $(call read_sources,commands)
THIRDPARTY_SRC := $(call read_sources,thirdparty)
MODULES_CORE_SRC := $(call read_sources,modules)

MODULE_SRC := $(MODULES_CORE_SRC)
MODULE_INCLUDES :=
MODULE_DEFS :=

ifeq ($(MODULE_EXPORT),1)
	MODULE_SRC += $(wildcard $(MODULES_DIR)/export/*.c)
	MODULE_INCLUDES += -I$(MODULES_DIR)/export
	MODULE_DEFS += -DTATR_MODULE_EXPORT
	COMMANDS_SRC += $(COMMANDS_DIR)/export.c
endif

ALL_SRC := $(MAIN_SRC) $(CORE_SRC) $(API_SRC) $(STORAGE_SRC) $(RENDER_SRC) $(COMMANDS_SRC) $(MODULE_SRC) $(THIRDPARTY_SRC)
OBJ := $(ALL_SRC:%.c=$(BUILD_DIR)/%.o)

WARN_FLAGS := \
	-Wall -Wextra \
	-Wshadow -Wconversion \
	-Wstrict-prototypes \
	-Werror=implicit-function-declaration

SAN_FLAGS :=

ifeq ($(WARNINGS),1)
	CFLAGS += $(WARN_FLAGS)
endif

ifeq ($(ASAN),1)
	SAN_FLAGS += -fsanitize=address
endif

ifeq ($(UBSAN),1)
	SAN_FLAGS += -fsanitize=undefined
endif

CFLAGS += $(SAN_FLAGS) $(MODULE_DEFS)
LDFLAGS += $(SAN_FLAGS)

ifeq ($(BUILD),Debug)
	CFLAGS += -O0 -ggdb
else ifeq ($(BUILD),Release)
	CFLAGS += -O3 -DNDEBUG
endif

INCLUDES := -I$(CORE_DIR) -I$(COMMANDS_DIR) -I$(THIRDPARTY_DIR) -I$(API_DIR) -I$(STORAGE_DIR) -I$(RENDER_DIR) $(MODULE_INCLUDES)

all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Compile
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Install
PREFIX ?= /usr/local
install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	cp $(TARGET) $(PREFIX)/bin/

# Info
info:
	@echo "Build type:     $(BUILD)"
	@echo "Warnings:       $(WARNINGS)"
	@echo "ASAN:           $(ASAN)"
	@echo "UBSAN:          $(UBSAN)"
	@echo "Module export:  $(MODULE_EXPORT)"

# Unit tests
UNIT_TEST_SRC := \
	tests/unit/test_libtatr.c \
	$(CORE_SRC) $(API_SRC) $(STORAGE_SRC) $(RENDER_SRC) $(MODULES_CORE_SRC) $(THIRDPARTY_SRC)

test-unit:
	@mkdir -p $(BUILD_DIR)
	$(CC) $(filter-out $(MODULE_DEFS),$(CFLAGS)) $(INCLUDES) $(UNIT_TEST_SRC) -o $(BUILD_DIR)/run_unit_tests
	@$(BUILD_DIR)/run_unit_tests

# Prints the fully-resolved set of sources this Makefile will compile,
# one per line, sorted. Used by scripts/check-build-drift.sh to compare
# against CMakeLists.txt's resolved set.
print-sources:
	@for f in $(sort $(ALL_SRC)); do echo $$f; done

.PHONY: info all clean install test-unit print-sources
