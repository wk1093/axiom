ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
include $(ROOT_DIR)/config.mk

# Artifacts
LIB_NAME = $(OBJDIR)/libax.a

# Source and Object Discovery
SRCS      = $(wildcard $(SRCDIR)/*.c)
OBJS      = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# Test Discovery
TEST_SRCS = $(wildcard tests/*.c)
TEST_EXES = $(patsubst tests/%.c, $(OBJDIR)/%_test, $(TEST_SRCS))

.PHONY: all tests clean

all: $(LIB_NAME)

# 1. Build the Static Library from everything in src/
$(LIB_NAME): $(OBJS)
	@mkdir -p $(OBJDIR)
	$(AR) rcs $@ $^

# 2. Compile Library Objects
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 3. Build all Test Executables
# Each test links against the already built libax.a
tests: $(LIB_NAME) $(TEST_EXES)
	@echo "--- All tests compiled in $(OBJDIR) ---"

$(OBJDIR)/%_test: tests/%.c $(LIB_NAME)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_NAME) -o $@
	@echo "Built test: $@"

clean:
	rm -rf $(OBJDIR)
