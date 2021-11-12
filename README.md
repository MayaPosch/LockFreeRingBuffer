# Lock-free ring buffer #

This is an implementation of a lock-free ring buffer, using only C++11 STL level features. Originally designed for the [NymphCast project](https://github.com/MayaPosch/NymphCast), it supports a single reader & writer model.

It can be used both for a continuous data stream, as well as for a specific data length (file length).

## Usage ##

The project is contained in a single static class (DataBuffer), which can be used as implemented in the test found in the `test/` folder.

More extensive documentation will follow.

## Test ##

In the `test/test_databuffer_multi_port.cpp` file a multi-threaded implementation is created that sets up the DataBuffer, starts a data request and data write thread, followed by starting a dummy reader that drives the constant reading from and writing to of data in the ring buffer.

This test requires C++14 (std::chrono features).
