OPENMP = #-fopenmp
TARGETS = linker cgt.so randcg link cgq
CXXPPFLAGS = -DUSE_EXPECT $(CXXINCLUDES)
CXXFLAGS = -Wall $(OPENMP) -g -O2 $(CXXPPFLAGS) -fPIC
LDFLAGS = $(OPENMP)

DIRS = . qlib
ALLSOURCES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cc $(dir)/*.hh $(dir)/*.ii)) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(patsubst %.cc,%.cc-dep,$(CCSOURCES))
CASES = $(patsubst cases/%.c,%,$(wildcard cases/*.c))

LDFLAGS += -lboost_regex -lreadline

all: $(TARGETS)

cgtmodule.% qlib/cgt-binding.%: CXXINCLUDES += -I/usr/include/python2.5/

linker: linker.o canon.o quark.o id.o symbol.o reader.o cgfile.o
randcg: randcg.o symbol.o quark.o id.o rand.o reader.o canon.o

cgt.so: LDFLAGS += -lboost_python -lpython2.5 -shared
cgt.so: qlib/cgt-binding.o qlib/Cgt.o qlib/Color.o -liberty
link: qlib/link.o qlib/Cgt.o qlib/Color.o -liberty
cgq: qlib/cgq.o qlib/Cgt.o qlib/Color.o -liberty

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

calgary.so: CXXFLAGS += -std=c++17
calgary.so: calgary.cc
	$(CXX) -I$(shell $(CXX) -print-file-name=plugin/include) $(CXXFLAGS) -shared -fpic -fno-rtti $^ -o $@

cgrtest-%: TEST_CFLAGS = $(shell grep ^//: ./cases/$*.c | cut -d' ' -f 2-)
cgrtest-%: CF = ./cases/$*.c
cgrtest-%: CGF = ./cases/$*.cg
cgrtest-%:
	@echo -n Test $*
	@$(CC) -c $(CF) $(TEST_CFLAGS) \
		-fplugin=$(shell pwd)/calgary.so -fplugin-arg-calgary-o=tmp
	@if [ -f $(CGF) ]; then			\
		echo -n " (diff)";		\
		diff -u $(CGF) tmp || exit 1;	\
	fi
	@echo
	@rm tmp
.PHONY: cgrtest-%

test: $(CASES:%=cgrtest-%)
	:

.PHONY: all clean dist
