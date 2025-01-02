# Makefile for the DataBuffer lock-free ring buffer project.


GNATMAKE = gnatmake
MAKEDIR = mkdir -p
RM = rm -f

BIN_OUTPUT := test_databuffer
BIN_OUTPUT_WR := test_databuffer_write_cases
ADAFLAGS := -aIsrc -D obj/ -gnat2022

TEST_SOURCES := reference/test_databuffer.adb
TEST_SOURCES_WR := reference/test_databuffer_write_cases.adb

all: makedir build

build:
	$(GNATMAKE) -o bin/$(BIN_OUTPUT) $(TEST_SOURCES) $(ADAFLAGS)
	$(GNATMAKE) -o bin/$(BIN_OUTPUT_WR) $(TEST_SOURCES_WR) $(ADAFLAGS)
	
makedir:
	$(MAKEDIR) bin
	$(MAKEDIR) obj

clean:
	rm -rf obj/
	rm -rf bin/
	
.PHONY: test src doc