build: entry.cpp
	clang++ -ltermbox -g -std=c++1z entry.cpp -o termbox-test

run: build
	./termbox-test

all: build
