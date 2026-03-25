CC      = aarch64-linux-gnu-gcc
AR      = aarch64-linux-gnu-ar
DBG     = qemu-aarch64 -g 1234
GDB	    = aarch64-linux-gnu-gdb -q -ex "target remote :1234" --args
RUNNER  = qemu-aarch64

ARCH := $(shell uname -m)

ifeq ($(ARCH), aarch64)
	CC      = gcc
	AR      = ar
	GDB     = gdb -q --args
	DBG     = gdb -q --args
	RUNNER  =
endif

CFLAGS  = -Wall -Wextra -O2 -static -g
INCLUDES = -I$(ROOT_DIR)/libax/include

SRCDIR  = src
INCDIR  = include
OBJDIR  = bin
