OPENMP = #-fopenmp
TARGETS = linker cgt.so randcg
CXXPPFLAGS = -DNDEBUG -DUSE_CPP0X -DUSE_EXPECT $(CXXINCLUDES)
CXXFLAGS = -std=c++0x -Wall $(OPENMP) -g -O2 $(CXXPPFLAGS) -fPIC
LDFLAGS = $(OPENMP)

ALLSOURCES = $(wildcard *.cc *.hh *.ii) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(CCSOURCES:%.cc=%.cc-dep)

LDFLAGS += -lboost_regex

all: $(TARGETS)

cgtmodule.cc-dep cgtmodule.o: CXXINCLUDES += -I/usr/include/python2.5/
cgt.so: LDFLAGS += -lboost_python -lpython2.5 -shared

linker: linker.o canon.o quark.o id.o symbol.o reader.o cgfile.o
cgt.so: cgtmodule.o canon.o quark.o id.o symbol.o reader.o cgfile.o
randcg: randcg.o symbol.o quark.o id.o rand.o reader.o canon.o

-include $(DEPFILES)

%.cc-dep: %.cc
	$(CXX) $(CXXFLAGS) -M $< > $@
$(TARGETS):
	$(CXX) $(LDFLAGS) $^ -o $@

test-%: %.o %.cc test.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DSELFTEST $(@:test-%=%.cc) $(filter-out $<,$(filter %.o,$^)) -o $@
	./$@ || (rm -f $@; exit 1)

clean:
	rm -f *.o *.cc-dep $(TARGETS)

.PHONY: all clean dist
