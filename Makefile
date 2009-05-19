OPENMP = #-fopenmp
TARGETS = linker cgt.so randcg link cgq
CXXPPFLAGS = -DNDEBUG -DUSE_CPP0X -DUSE_EXPECT $(CXXINCLUDES)
CXXFLAGS = -std=c++0x -Wall $(OPENMP) -g -O2 $(CXXPPFLAGS) -fPIC
LDFLAGS = $(OPENMP)

DIRS = . qlib
ALLSOURCES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cc $(dir)/*.hh $(dir)/*.ii)) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(patsubst %.cc,%.cc-dep,$(CCSOURCES))

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

.PHONY: all clean dist
