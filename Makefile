# Makefile for lock-free ring buffer multi-port test scenario.
#
# 2021/11/11, Maya Posch


CPPFLAGS := -std=c++14 -g3 -O0 -pthread

all: test_databuffer_mport

makedirs:
	mkdir -p bin

test_databuffer_mport: makedirs
	g++ -o bin/test_db_mp -I. -Isrc test/test_databuffer_multi_port.cpp src/databuffer.cpp test/chronotrigger.cpp test/readdummy.cpp $(CPPFLAGS)