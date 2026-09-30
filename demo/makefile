BUILD    := build
SRC_DIR  := ../src
INC_DIR  := ../include
LTDIR    := ../deps/linc_tools

CC       := clang

CFLAGS   := -g -O0 -std=gnu23 -Wall -Wextra -fcolor-diagnostics
CPPFLAGS := -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/input -I$(LTDIR)
CPPFLAGS += -D_GNU_SOURCE
DEPFLAGS := -MMD -MP
CC_FLAGS := $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS)

LDLIBS := -lunibilium

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.d,$(SRCS))

BINS += cursor_movement

.PHONY: all clean

all: $(BINS)

# cursor_movement
cursor_movement: $(BUILD)/cursor_movement.o $(OBJS)
	$(CC) $(LDLIBS) $^ -o $(BUILD)/$@
	valgrind --quiet ./build/cursor_movement 2>out

$(BUILD)/cursor_movement.o: cursor_movement.c
	mkdir -p build
	$(CC) $(CC_FLAGS) $< -c -o $@

# all
$(BUILD)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CC_FLAGS) $< -c -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPS)
