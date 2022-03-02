CXXFLAGS = $(shell pkg-config --cflags libpmemobj++) -std=c++11 -O2 -pthread -mavx2 -march=native -fopenmp -DD_GLIBCXX_PARALLEL

LDFLAGS = $(shell pkg-config --libs libpmemobj++) -O2 -pthread -mavx2 -march=native -fopenmp -DD_GLIBCXX_PARALLEL

# bindex: bindex.cc
# 	g++ -std=c++11 $^ -o $@ -pthread -mavx2 -march=native -fopenmp -DD_GLIBCXX_PARALLEL
#

CXX = g++

# all:$(subst .c,.o,$(wildcard *.c))
all:bindex_utils.o bindex_background_init.o bindex_remove.o bindex_build.o bindex_search.o bindex_insert.o main.o bindex

%.o: %.cpp
	$(CXX) -c $^ -o $@ $(CXXFLAGS) -pg


# for call graph
instrument.o: instrument.c
	$(CXX) -c $^ -o $@ -g -finstrument-functions

bindex: main.o bindex_build.o bindex_search.o bindex_insert.o bindex_remove.o bindex_background_init.o bindex_utils.o instrument.o
	$(CXX) -o $@ $^ $(LDFLAGS) -pg

clean:
	rm -rf bindex bindex_utils.o bindex_build.o bindex_search.o bindex_insert.o main.o bindex_remove.o bindex_background_init.o
