# Lock-Free Ring Buffer #

This directory contains the Ada port of the C++-based Lock-Free Ring Buffer (see top folder of project). It is still a work in progress and does not contain all of the features yet.

## Features ##

At this point in development, the lock-free ring buffer (LFRB) has the following features:

1. Setting the size of the buffer to use (in bytes, on the heap) with the `init` function.
2. Deleting the allocated buffer (with `cleanup`).
3. Registering a data request task (with `setDataRquestTask`).
4. Writing into the buffer (with `write`).
5. Read data from the buffer (with `read`).

All of the read and write actions can be performed without affecting the other. This is enabled by the lock-free design that prevents write actions from overwriting unread data.

## Usage ##

The basic usage of LFRB is detailed in the `reference/test_databuffer.adb` file, with the `Makefile` in this folder allowing for this reference design to be compiled to a binary found under `bin/`.

For a new project, at a minimum the `src/lfringdatabuffer.*` and `reference/dataRequest.ads` files are required, along with a custom implementation of `reference/dataRequest`. 

The `dataRequest` package provides a task type (`dataRequestTask`) which implements the rendezvous interface. It has a single entry called `fetch` which gets called when the buffer is nearly empty, and which is expected to write fresh data into the buffer.

## Design ##

The design of the LFRB consists out of a single package containing the core methods and variables. An accompanying `dataRequest`package contains the `dataRequestTask` which is used to implement the data writer. Although multiple readers can exist, it's generally assumed that there's only a single reader to match the single writer.


