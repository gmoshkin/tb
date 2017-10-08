CXX=clang++-4.0
EXECUTABLE=./termbox-test
CXX_OPTIONS=-ltermbox -g -Wall -std=c++1z

build: entry.cpp
	$(CXX) $(CXX_OPTIONS) entry.cpp -o $(EXECUTABLE)

run: build
	$(EXECUTABLE)

clean:
	rm $(EXECUTABLE)

all: build
