TARGETS = linker randcg calgary.so
CXXPPFLAGS = -DUSE_EXPECT $(CXXINCLUDES)
CXXFLAGS = -Wall -g -O2 $(CXXPPFLAGS) -fPIC -std=c++17
LDFLAGS =

DIRS = .
ALLSOURCES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cc $(dir)/*.hh $(dir)/*.ii)) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(patsubst %.cc,%.cc-dep,$(CCSOURCES))
CASES = $(patsubst cases/%.c,%,$(wildcard cases/*.c))
TESTS = $(patsubst cases/%.sh,%,$(wildcard cases/test-*.sh))

all: $(TARGETS)

linker: linker.o canon.o id.o symbol.o reader.o cgfile.o
randcg: randcg.o symbol.o id.o rand.o reader.o canon.o

calgary.o: CXXPPFLAGS += -I$(shell $(CXX) -print-file-name=plugin/include) \
			  -fpic -fno-rtti
calgary.so: LDFLAGS += -shared -fpic -fno-rtti
calgary.so: calgary.o

-include $(DEPFILES)

%.cc-dep: %.cc
	$(CXX) $(CXXFLAGS) -MM -MT '$(<:%.cc=%.o) $@' $< > $@
$(TARGETS):
	$(CXX) $(LDFLAGS) $^ -o $@

test-%: %.o %.cc test.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DSELFTEST $(@:test-%=%.cc) $(filter-out $<,$(filter %.o,$^)) -o $@
	./$@ || (rm -f $@; exit 1)

clean:
	rm -f *.o qlib/*.o qlib/*.*-dep *.*-dep $(TARGETS)

cgrtest-%: TEST_CFLAGS = $(shell grep ^//: ./cases/$*.c | cut -d' ' -f 2-)
cgrtest-%: CF = ./cases/$*.c
cgrtest-%: CGF = ./cases/$*.cg
cgrtest-%: LCGF = ./cases/$*.lcg
cgrtest-%:
	@echo -n Test cgr $*
	@$(CC) -c $(CF) $(TEST_CFLAGS) \
		-fplugin=$(shell pwd)/calgary.so -o tmp
	@objcopy --dump-section=.calgary.callgraph=tmp2 tmp
	@if [ -f $(CGF) ]; then			\
		echo -n " (diff)";		\
		diff -u $(CGF) tmp2 || exit 1;	\
	fi
	@./linker tmp2 -o tmp3
	@if [ -f $(LCGF) ]; then		\
		echo -n " (link diff)";		\
		diff -u $(LCGF) tmp3 || exit 2;	\
	fi
	@echo
	@rm tmp tmp2 tmp3
.PHONY: cgrtest-%

test-%:
	@echo Test sh $*
	@(cd cases; ./$*.sh)

check-cgr: $(CASES:%=cgrtest-%)
check-tests: $(TESTS:%=test-%)

check: check-cgr check-tests

.PHONY: all clean dist
