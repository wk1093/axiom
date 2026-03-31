include config.mk

SUBDIRS = axas axld libax axlibc axcc

.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf bin

# for all .S files in the tests/ directory, use the axas assembler to convert them to .o files in the bin/ directory
# and then link them with the axld linker to create executable files in the bin/ directory
# we will have different ways to build them
# we choose which compiler: axas or gcc
# and which linker: axld, gcc, or ld (gcc uses ld, but with stdlib, ld is for testing non-stdlib linking without axld)
bin:
	mkdir -p bin

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Standalone tests — define their own _start and do not use stdlib.
# Assembled with:  axas  or  gcc -c
# Linked with:     axld  or  system ld
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Object files — prefixed with "standalone_" to avoid collisions with stdlib objs.
# Note: % in Make pattern rules never matches /, so these rules only apply to
# single files directly under tests/standalone/, not subdirectories.
bin/standalone_%_axas.o: tests/standalone/%.S axas | bin
	-$(RUNNER) axas/bin/axas -o $@ $<
bin/standalone_%_gcc.o: tests/standalone/%.S | bin
	-$(CC) $(CFLAGS) -c $< -o $@

# Executables
bin/standalone_%_axas_axld: bin/standalone_%_axas.o axld | bin
	-$(RUNNER) axld/bin/axld -o $@ $<
	-chmod +x $@
bin/standalone_%_gcc_axld: bin/standalone_%_gcc.o axld | bin
	-$(RUNNER) axld/bin/axld -o $@ $<
	-chmod +x $@
bin/standalone_%_gcc_ld: bin/standalone_%_gcc.o | bin
	-$(LD) -o $@ $<
bin/standalone_%_axas_ld: bin/standalone_%_axas.o | bin
	-$(LD) -o $@ $<

STANDALONE_SINGLES := $(patsubst tests/standalone/%.S,%,$(wildcard tests/standalone/*.S))
STANDALONE_SINGLE_EXES := \
	$(patsubst %,bin/standalone_%_axas_axld,$(STANDALONE_SINGLES)) \
	$(patsubst %,bin/standalone_%_gcc_axld, $(STANDALONE_SINGLES)) \
	$(patsubst %,bin/standalone_%_gcc_ld,   $(STANDALONE_SINGLES))

# Multi-object standalone subdirectories.
# Intermediates go into bin/standalone_$(dir)/ to avoid pattern rule clashes.
STANDALONE_MULTI_DIRS := $(patsubst tests/standalone/%/,%,$(wildcard tests/standalone/*/))
STANDALONE_MULTI_EXES :=

define STANDALONE_MULTI_template
$(1)_GCC_OBJS  := $(patsubst tests/standalone/$(1)/%.S,bin/standalone_$(1)/%_gcc.o,$(wildcard tests/standalone/$(1)/*.S))
$(1)_AXAS_OBJS := $(patsubst tests/standalone/$(1)/%.S,bin/standalone_$(1)/%_axas.o,$(wildcard tests/standalone/$(1)/*.S))

bin/standalone_$(1):
	mkdir -p $$@

bin/standalone_$(1)/%_gcc.o: tests/standalone/$(1)/%.S | bin/standalone_$(1)
	-$$(CC) $$(CFLAGS) -c $$< -o $$@

bin/standalone_$(1)/%_axas.o: tests/standalone/$(1)/%.S axas | bin/standalone_$(1)
	-$$(RUNNER) axas/bin/axas -o $$@ $$<

bin/standalone_$(1)_gcc_axld: $$($(1)_GCC_OBJS) axld | bin
	-$$(RUNNER) axld/bin/axld -o $$@ $$($(1)_GCC_OBJS)
	-chmod +x $$@

bin/standalone_$(1)_axas_axld: $$($(1)_AXAS_OBJS) axld | bin
	-$$(RUNNER) axld/bin/axld -o $$@ $$($(1)_AXAS_OBJS)
	-chmod +x $$@

bin/standalone_$(1)_gcc_ld: $$($(1)_GCC_OBJS) | bin
	-$$(LD) -o $$@ $$($(1)_GCC_OBJS)

bin/standalone_$(1)_axas_ld: $$($(1)_AXAS_OBJS) | bin
	-$$(LD) -o $$@ $$($(1)_AXAS_OBJS)

STANDALONE_MULTI_EXES += bin/standalone_$(1)_gcc_axld bin/standalone_$(1)_axas_axld bin/standalone_$(1)_gcc_ld bin/standalone_$(1)_axas_ld
endef

$(foreach dir,$(STANDALONE_MULTI_DIRS),$(eval $(call STANDALONE_MULTI_template,$(dir))))

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Stdlib tests
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bin/stdlib_%_gcc.o: tests/stdlib/%.S | bin
	-$(CC) $(CFLAGS) -c $< -o $@

bin/stdlib_%_axas.o: tests/stdlib/%.S axas | bin
	-$(RUNNER) axas/bin/axas -o $@ $<

bin/stdlib_%_gcc_ld: bin/stdlib_%_gcc.o | bin
	-$(CC) $(CFLAGS) $< -o $@

bin/stdlib_%_axas_axld: bin/stdlib_%_axas.o | bin axlibc
	-$(RUNNER) axld/bin/axld -o $@ axlibc/bin/libc.o $<
	-chmod +x $@

STDLIB_SINGLES := $(patsubst tests/stdlib/%.S,%,$(wildcard tests/stdlib/*.S))
STDLIB_SINGLE_EXES := $(patsubst %,bin/stdlib_%_gcc_ld,$(STDLIB_SINGLES)) $(patsubst %,bin/stdlib_%_axas_axld,$(STDLIB_SINGLES))

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# Test runner
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ALL_TEST_EXES := $(STANDALONE_SINGLE_EXES) $(STANDALONE_MULTI_EXES) $(STDLIB_SINGLE_EXES)

test: $(ALL_TEST_EXES)
	@echo "--- All tests built ---"
	@for exe in $(ALL_TEST_EXES); do \
		if [ -x $$exe ]; then \
			if $$exe; then \
				echo "PASS: $$exe"; \
			else \
				echo "FAIL: $$exe"; \
			fi; \
		else \
			echo "SKIP: $$exe (not built)"; \
		fi; \
	done
