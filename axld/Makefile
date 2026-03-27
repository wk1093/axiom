ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
include $(ROOT_DIR)/config.mk

TARGET = $(OBJDIR)/axld
LIB_PATH = $(ROOT_DIR)/libax/$(OBJDIR)/libax.a

# add include for this project
INCLUDES += -I$(ROOT_DIR)/axld/include -I$(ROOT_DIR)/axas/include

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

all: $(TARGET)

libax:
	$(MAKE) -C $(ROOT_DIR)/libax

# Link against libax
$(TARGET): $(OBJS) $(LIB_PATH) libax
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -L$(ROOT_DIR)/libax/$(OBJDIR) -lax

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_PATH):
	$(MAKE) -C $(ROOT_DIR)/libax

clean:
	rm -rf $(OBJDIR)
	$(MAKE) -C $(ROOT_DIR)/libax clean

test: $(TARGET)
	$(TARGET) ../axas/bin/test2.o -o bin/test2

dbg: $(TARGET)
	$(DBG) $(TARGET) ../axas/bin/test2.o -o bin/test2

gdb: $(TARGET)
	$(GDB) $(TARGET)