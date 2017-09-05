CXX=clang++-4.0

build: entry.cpp
	$(CXX) -ltermbox -g -Wall -std=c++1z entry.cpp -o termbox-test

run: build
	./termbox-test

all: build
