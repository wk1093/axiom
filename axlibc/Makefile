ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
include $(ROOT_DIR)/config.mk

TARGET = $(OBJDIR)/libc.a

SRCS = $(wildcard $(SRCDIR)/*.S)
OBJS = $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.o,$(SRCS))

all: $(TARGET)

axas:
	$(MAKE) -C $(ROOT_DIR)/axas

axar:
	$(MAKE) -C $(ROOT_DIR)/axar

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJS) axar $(OBJDIR)
	-$(RUNNER) $(ROOT_DIR)/axar/bin/axar $(TARGET) $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.S | axas $(OBJDIR)
	-$(RUNNER) $(ROOT_DIR)/axas/bin/axas -o $@ $<

clean:
	rm -rf $(OBJDIR)
	$(MAKE) -C $(ROOT_DIR)/axas clean
