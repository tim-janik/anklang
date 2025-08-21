# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

all: testjsonipc

builddir ?= out
CXX	 = ccache clang++
#CXX	 = ccache g++ -Wno-nonnull-compare -Wno-address
CXXFLAGS = -std=gnu++17 -Wall -O0 -g
OBJECTS  = testjsonipc.o
DEFS     = -DSTANDALONE
DEPS     = jsonipc.hh Makefile

INCLUDES = -I../external/rapidjson/include/

$(OBJECTS): %.o: %.cc $(DEPS)
	$(CXX) $(DEFS) $(CXXFLAGS) $(INCLUDES) -c $<
.INTERMEDIATE: $(OBJECTS)

testjsonipc: $(OBJECTS) Makefile
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -lboost_system
.INTERMEDIATE: testjsonipc

checkts:
	../node_modules/.bin/tsc --noEmit --allowJs -m esnext --target esnext \
		--moduleResolution bundler --erasableSyntaxOnly \
		jsonbindings.ts jsonipc.ts

check: testjsonipc checkts
	./testjsonipc

print: testjsonipc
	./testjsonipc --print

clean:
	rm -f $(OBJECTS) testjsonipc
