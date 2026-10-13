BUILD   := build
BIN     := $(BUILD)/test_all

SRC_DIR    := src
INC_DIR    := include
TEST_DIR   := test
LTDIR      := deps/linc_tools
UDIR       := deps/Unity

# compiler
CC       := clang
CFLAGS   := -g -O0 -std=gnu23 -Wall -Wextra -fcolor-diagnostics
CPPFLAGS := -I$(TEST_DIR) -I$(SRC_DIR) -I$(LTDIR) -I$(UDIR) -I$(INC_DIR)
CPPFLAGS += -D_GNU_SOURCE -DUNITY_OUTPUT_COLOR -DUNITY_FIXTURE_NO_EXTRAS
DEPFLAGS := -MMD -MP
CC_FLAGS := $(CFLAGS) $(CPPFLAGS) $(DFLAGS) $(DEPFLAGS)

# linker
LDFLAGS  :=
LDLIBS   := -lunibilium -lvterm -lpthread
LD_FLAGS := $(LDFLAGS) $(LDLIBS)

# src/obj/dep

SRCS   := $(wildcard $(SRC_DIR)/*.c)
SRCS   += $(wildcard $(UDIR)/*.c)
SRCS   += $(wildcard $(TEST_DIR)/*.c)

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(patsubst %.c,$(BUILD)/%.d,$(SRCS))

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LD_FLAGS) $^ -o $@

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPS)
