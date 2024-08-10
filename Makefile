# Makefile for lock-free ring buffer multi-port test scenario.
#
# 2021/11/11, Maya Posch


CPPFLAGS := -std=c++14 -g3 -O0 -pthread

all: makedirs test_databuffer_mport test_databuffer_write_cases

makedirs:
	mkdir -p bin

test_databuffer_mport:
	g++ -o bin/test_db_mp -I. -Isrc test/test_databuffer_multi_port.cpp src/databuffer.cpp test/chronotrigger.cpp test/readdummy.cpp $(CPPFLAGS)
	
test_databuffer_write_cases:
	g++ -o bin/test_db_cases -I. -Isrc test/test_databuffer_write_cases.cpp src/databuffer.cpp test/chronotrigger.cpp test/readdummy.cpp $(CPPFLAGS)
	