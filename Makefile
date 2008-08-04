TARGETS = test-canon linker query cgt.so
CXXFLAGS = -std=c++0x -Wall -g -O2 -DNDEBUG -DUSE_CPP0X -UUSE_EXPECT $(CXXINCLUDES)

ALLSOURCES = $(wildcard *.cc *.hh *.ii) Makefile
CCSOURCES = $(filter %.cc,$(ALLSOURCES))
DEPFILES = $(CCSOURCES:%.cc=%.cc-dep)

all: $(TARGETS)

$(TARGETS): LDFLAGS += -lstdc++

cgtmodule.cc-dep cgtmodule.o: CXXINCLUDES += -I/usr/include/python2.5/
cgt.so: LDFLAGS += -lboost_python -lpython2.5 -shared -fpic

linker: linker.o canon.o quark.o id.o symbol.o reader.o cgfile.o
query: query.o canon.o quark.o id.o symbol.o reader.o cgfile.o
cgt.so: cgtmodule.o canon.o quark.o id.o symbol.o reader.o cgfile.o
	$(CC) $(LDFLAGS) $^ -o $@

-include $(DEPFILES)

%.cc-dep: %.cc
	$(CXX) $(CXXINCLUDES) -M $< > $@

test-%: %.o %.cc test.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DSELFTEST $(@:test-%=%.cc) $(filter-out $<,$(filter %.o,$^)) -o $@
	./$@ || (rm -f $@; exit 1)

clean:
	rm -f *.o *.cc-dep $(TARGETS)

.PHONY: all clean dist
