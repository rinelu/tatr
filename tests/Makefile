CC       ?= cc
CFLAGS   ?= -g
TATR_BIN ?= tatr

LIB_DIR = ../libs

LIB_SRCS := \
    $(LIB_DIR)/astring.c \
    $(LIB_DIR)/fs.c \
    $(LIB_DIR)/temp.c

TEST_SRCS := \
    main.c \
    test_init_new.c \
    test_list_show_edit.c \
    test_lifecycle_org.c \
    test_attachments.c \
    test_status_meta.c

TARGET := run_tests

.PHONY: all build run clean

all: build run

build: $(TARGET)

$(TARGET): $(TEST_SRCS) $(LIB_SRCS) test_framework.h test_helpers.h
	$(CC) $(CFLAGS) -I$(LIB_DIR) -DTATR_BIN='"$(TATR_BIN)"' \
	    -o $@ $(TEST_SRCS) $(LIB_SRCS)

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -f $(TARGET)
