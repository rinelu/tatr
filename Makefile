CC := cc
CFLAGS :=
TARGET := tatr
BUILD ?= Debug

# Options (set to 1 to enable)
WARNINGS ?= 1
ASAN ?= 0
UBSAN ?= 0

SRC_DIR := src
CMD_DIR := src/cmd
LIB_DIR := libs
BUILD_DIR := build

# Sources
SRC := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/issue.c \
	$(SRC_DIR)/ui.c

CMD_SRC := \
	$(CMD_DIR)/basic.c \
	$(CMD_DIR)/init.c \
	$(CMD_DIR)/new.c \
	$(CMD_DIR)/list.c \
	$(CMD_DIR)/show.c \
	$(CMD_DIR)/edit.c \
	$(CMD_DIR)/close.c \
	$(CMD_DIR)/delete.c \
	$(CMD_DIR)/comment.c \
	$(CMD_DIR)/search.c \
	$(CMD_DIR)/reopen.c \
	$(CMD_DIR)/attach.c \
	$(CMD_DIR)/attachls.c \
	$(CMD_DIR)/detach.c \
	$(CMD_DIR)/tag.c \
	$(CMD_DIR)/export.c \
	$(CMD_DIR)/status.c

LIB_SRC := \
	$(LIB_DIR)/fs.c \
	$(LIB_DIR)/astring.c \
	$(LIB_DIR)/temp.c

ALL_SRC := $(SRC) $(CMD_SRC) $(LIB_SRC)
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

CFLAGS += $(SAN_FLAGS)
LDFLAGS += $(SAN_FLAGS)

ifeq ($(BUILD),Debug)
	CFLAGS += -O0 -ggdb
else ifeq ($(BUILD),Release)
	CFLAGS += -O3 -DNDEBUG
endif

INCLUDES := -I$(SRC_DIR) -I$(CMD_DIR) -I$(LIB_DIR)

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
	@echo "Build type: $(BUILD)"
	@echo "Warnings:   $(WARNINGS)"
	@echo "ASAN:       $(ASAN)"
	@echo "UBSAN:      $(UBSAN)"

.PHONY: info all clean install
