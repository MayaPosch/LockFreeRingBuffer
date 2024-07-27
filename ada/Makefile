# Makefile for the DataBuffer lock-free ring buffer project.


GNATMAKE = gnatmake
MAKEDIR = mkdir -p
RM = rm -f

BIN_OUTPUT := test_databuffer
ADAFLAGS := -aIsrc -D obj/

TEST_SOURCES := reference/test_databuffer.adb

all: makedir build

build:
	$(GNATMAKE) -o bin/$(BIN_OUTPUT) $(TEST_SOURCES) $(ADAFLAGS)
	
makedir:
	$(MAKEDIR) bin
	$(MAKEDIR) obj

clean:
	rm -rf obj/
	rm -rf bin/
	
.PHONY: test src doc