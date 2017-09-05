CXX=clang++-4.0
EXECUTABLE=./termbox-test

build: entry.cpp
	$(CXX) -ltermbox -g -Wall -std=c++1z entry.cpp -o termbox-test

run: build
	$(EXECUTABLE)

clean:
	rm $(EXECUTABLE)

all: build
