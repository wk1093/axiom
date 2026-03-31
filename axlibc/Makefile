ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
include $(ROOT_DIR)/config.mk

TARGET = $(OBJDIR)/libc.o

SRCS = $(wildcard $(SRCDIR)/*.S)

all: $(TARGET)

axas:
	$(MAKE) -C $(ROOT_DIR)/axas

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(SRCS) axas $(OBJDIR)
	$(ROOT_DIR)/axas/bin/axas $(SRCS) -o $(TARGET)

clean:
	rm -rf $(OBJDIR)
	$(MAKE) -C $(ROOT_DIR)/axas clean
