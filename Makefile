OPENMP = #-fopenmp
TARGETS = linker cgt.so randcg link cgq calgary.so
CXXPPFLAGS = -DUSE_EXPECT $(CXXINCLUDES)
CXXFLAGS = -Wall $(OPENMP) -g -O2 $(CXXPPFLAGS) -fPIC -std=c++17
LDFLAGS = $(OPENMP)

DIRS = . qlib
ALLSOURCES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cc $(dir)/*.hh $(dir)/*.ii)) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(patsubst %.cc,%.cc-dep,$(CCSOURCES))
CASES = $(patsubst cases/%.c,%,$(wildcard cases/*.c))
TESTS = $(patsubst cases/%.sh,%,$(wildcard cases/test-*.sh))

all: $(TARGETS)

cgtmodule.% qlib/cgt-binding.%: CXXINCLUDES += -I/usr/include/python2.5/

linker: linker.o canon.o quark.o id.o symbol.o reader.o cgfile.o
randcg: randcg.o symbol.o quark.o id.o rand.o reader.o canon.o

cgt.so: LDFLAGS += -lboost_python -lpython2.5 -shared
cgt.so: qlib/cgt-binding.o qlib/Cgt.o qlib/Color.o -liberty
link: qlib/link.o qlib/Cgt.o qlib/Color.o -liberty
cgq: qlib/cgq.o qlib/Cgt.o qlib/Color.o -liberty

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
cgrtest-%:
	@echo -n Test cgr $*
	@$(CC) -c $(CF) $(TEST_CFLAGS) \
		-fplugin=$(shell pwd)/calgary.so -o tmp
	@if [ -f $(CGF) ]; then			\
		echo -n " (diff)";		\
		objcopy --dump-section=.calgary.callgraph=/dev/stdout tmp \
			| diff -u $(CGF) /dev/stdin || exit 1;	\
	fi
	@echo
	@rm tmp
.PHONY: cgrtest-%

test-%:
	@echo Test sh $*
	@(cd cases; ./$*.sh)

check-cgr: $(CASES:%=cgrtest-%)
check-tests: $(TESTS:%=test-%)

check: check-cgr check-tests

.PHONY: all clean dist
