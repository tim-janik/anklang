all: testjsonipc

builddir ?= out
CXX	 = ccache clang++
#CXX	 = ccache g++ -Wno-nonnull-compare -Wno-address
CXXFLAGS = -std=gnu++17 -Wall -O0 -g
OBJECTS  = testjsonipc.o
DEFS     = -DSTANDALONE -D__JSONIPC_NULL_REFERENCE_THROWS__
DEPS     = jsonipc.hh Makefile

INCLUDES = -I../$(builddir)/external/

$(OBJECTS): %.o: %.cc $(DEPS)
	$(CXX) $(DEFS) $(CXXFLAGS) $(INCLUDES) -c $<
.INTERMEDIATE: $(OBJECTS)

testjsonipc: $(OBJECTS) Makefile
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -lboost_system
.INTERMEDIATE: testjsonipc

check: testjsonipc
	./testjsonipc

clean:
	rm -f $(OBJECTS) testjsonipc
