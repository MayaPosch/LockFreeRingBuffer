/*
	test_databuffer_multi_port.cpp - DataBuffer test with simultaneous read & write.
	
*/


#include "../src/databuffer.h"
#include "readdummy.h"

#include <csignal>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>


std::condition_variable gCon;
std::mutex gMutex;

uint32_t chunk_size = 200 * 1024; // 200 kB
const char* chunk;
std::thread* readThread;

std::atomic<bool> running = { true };
std::atomic<bool> readerRunning = { false };
std::condition_variable dataRequestCv;
std::mutex dataRequestMtx;
std::condition_variable dataWriteCv;
std::mutex dataWriteMtx;


// --- DATA REQUEST FUNCTION ---
// This function can be signalled with the condition variable to request data from the client.
void dataRequestFunction() {
	// Create and share condition variable with DataBuffer class.
	DataBuffer::setDataRequestCondition(&dataRequestCv);
	
	while (running) {
		// Wait for the condition to be signalled.
		std::unique_lock<std::mutex> lk(dataRequestMtx);
		dataRequestCv.wait(lk);
		
		if (!running) {
			std::cout << "Shutting down data request function..." << std::endl;
			break;
		}
		else if (!DataBuffer::dataRequestPending) {
			std::cout << "Spurious wakeup." << std::endl;
			continue;  // Spurious wake-up.
		}
		
		std::cout << "Asking for data..." << std::endl;
		
		// Trigger write.
		dataWriteCv.notify_one();
		
		if (!readerRunning) {
			// Start the reader dummy thread.
			readThread = new std::thread(ReadDummy::run);
			readerRunning = true;
		}
	}
}


// --- WRITE BUFFER ---
// Write data into the buffer when signalled.
void dataWriteFunction() {
	while (running) {
		// Wait for the condition to be signalled.
		std::unique_lock<std::mutex> lk(dataWriteMtx);
		dataWriteCv.wait(lk);
		
		if (!running) {
			std::cout << "Shutting down data write function..." << std::endl;
			break;
		}
	
		// Write into buffer after a brief delay.
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		DataBuffer::write(chunk, chunk_size);
	}
}


// --- SEEKING HANDLER ---
void seekingHandler(uint32_t session, int64_t offset) {
	if (DataBuffer::seeking()) {
		std::cout << "Seeking..." << std::endl;
	
		// Write into buffer after a brief delay.
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		DataBuffer::write(chunk, chunk_size);
	}
}


void signal_handler(int signal) {
	std::cout << "SIGINT handler called. Shutting down..." << std::endl;
	gCon.notify_one();
}


int main() {
	// Set up buffer.
	chunk = new char[chunk_size];
	
	// Init DataBuffer.
	uint32_t buffer_size = 1 * (1024 * 1024); // 1 MB
	DataBuffer::init(buffer_size);
	DataBuffer::setSeekRequestCallback(seekingHandler);
	
	// Start the data write handler in its own thread.
	std::thread writeThread(dataWriteFunction);
	
	// Start the data request handler in its own thread.
	std::thread drq(dataRequestFunction);
	
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	if (!DataBuffer::start()) {
		std::cerr << "DataBuffer start failed." << std::endl;
		return 1;
	}
	
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	// Wait here until SIGINT.
	signal(SIGINT, signal_handler);
	
	std::unique_lock<std::mutex> lk(gMutex);
	gCon.wait(lk);
	
	std::cout << "Exiting..." << std::endl;
	
	// Clean-up.
	running = false;
	ReadDummy::quit();
	DataBuffer::cleanup();
	dataRequestCv.notify_one();
	dataWriteCv.notify_one();
	drq.join();
	writeThread.join();
	readThread->join();
	delete readThread;
	
	delete[] chunk;
	
	return 0;
}
