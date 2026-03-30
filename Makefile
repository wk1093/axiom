include config.mk

SUBDIRS = axas axld libax

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

# compile options
bin/%_axas.o: tests/%.S axas bin
	-$(RUNNER) axas/bin/axas -o $@ $<
bin/%_gcc.o: tests/%.S bin
	-$(CC) $(CFLAGS) -c $< -o $@

# link options
bin/%_axld: bin/%.o axld bin
	-$(RUNNER) axld/bin/axld -o $@ $<
	-chmod +x $@
bin/%_gcc: bin/%.o bin
	-$(CC) $(CFLAGS) $< -o $@
bin/%_ld: bin/%.o bin
	-$(LD) -o $@ $<

# make a test target that builds all the test executables using axas and axld (some will fail because they are meant to be run with or without cstdlib, so we ignore failures)
ALL_TESTS = $(wildcard tests/*.S)
TEST_OBJECTS = $(patsubst tests/%.S, bin/%_axas.o, $(ALL_TESTS)) $(patsubst tests/%.S, bin/%_gcc.o, $(ALL_TESTS))
TEST_EXECUTABLES = $(patsubst bin/%.o, bin/%_axld, $(TEST_OBJECTS)) $(patsubst bin/%.o, bin/%_gcc, $(TEST_OBJECTS)) $(patsubst bin/%.o, bin/%_ld, $(TEST_OBJECTS))
test: $(TEST_EXECUTABLES)
	@echo "--- All tests built in bin/ ---"
	@# Run all of the ones that were built without failure, ignoring the ones that were meant to fail
	@# and generate a report of which tests passed and which failed
	@for exe in $(TEST_EXECUTABLES); do \
		if [ -x $$exe ]; then \
			echo "Running $$exe..."; \
			if $$exe; then \
				echo "PASS: $$exe"; \
			else \
				echo "FAIL: $$exe"; \
			fi; \
		else \
			echo "SKIP: $$exe (not built)"; \
		fi; \
	done
