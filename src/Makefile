BINARY=gqrx-remote
SOURCES=main.cpp GqrxConnection.cpp XlibKeyConnection.cpp
OBJECTS=$(SOURCES:.cpp=.o)

CXX=g++
CXXFLAGS+=-I. -ggdb -std=c++17
LDFLAGS=-lX11

all: $(BINARY)

DEPFLAGS+=-I/usr/include/c++/4.4/ -I/usr/include/c++/4.4/i486-linux-gnu -I/usr/lib/gcc/i486-linux-gnu/4.4/include -I/usr/include/c++/4.4/backward/
depend:
		makedepend -- $(CXXFLAGS) $(DEPFLAGS) -- $(SOURCES)

$(BINARY): $(OBJECTS)
		$(CXX) $^ $(LDFLAGS) -o $@

clean:
		rm -rf $(OBJECTS) core $(BINARY)

# DO NOT DELETE
