build: entry.cpp
	clang++ -ltermbox -g -std=c++14 entry.cpp -o termbox-test

run: build
	./termbox-test

all: build
