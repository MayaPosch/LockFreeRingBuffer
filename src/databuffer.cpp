/*
	databuffer.cpp - Implementation of the DataBufer class.
	
	Revision 0.
	
	Notes:
			- 
			
	2020/11/19, Maya Posch
*/


//#define DEBUG 1

#include "databuffer.h"

#include <cstring>
#include <chrono>
#include <thread>
#ifdef DEBUG
#include <iostream>
#endif


// Static initialisations.
uint8_t* DataBuffer::buffer = 0;
uint8_t* DataBuffer::end = 0;
uint8_t* DataBuffer::front = 0;
uint8_t* DataBuffer::back = 0;
uint8_t* DataBuffer::index = 0;
uint32_t DataBuffer::capacity = 0;
uint32_t DataBuffer::size = 0;
int64_t DataBuffer::filesize = 0;
std::atomic<uint32_t> DataBuffer::unread = { 0 };
std::atomic<uint32_t> DataBuffer::free = { 0 };
uint32_t DataBuffer::byteIndex = 0;
uint32_t DataBuffer::byteIndexLow = 0;
uint32_t DataBuffer::byteIndexHigh = 0;
std::atomic<bool> DataBuffer::eof = { false };
std::atomic<DataBuffer::BufferState> DataBuffer::state;
SeekRequestCallback DataBuffer::seekRequestCallback = 0;
std::condition_variable* DataBuffer::dataRequestCV = 0;
std::mutex DataBuffer::dataWaitMutex;
std::condition_variable DataBuffer::dataWaitCV;
std::atomic<bool> DataBuffer::dataRequestPending = { false };
std::mutex DataBuffer::seekRequestMutex;
std::condition_variable DataBuffer::seekRequestCV;
std::atomic<bool> DataBuffer::seekRequestPending = { false };
uint32_t DataBuffer::sessionHandle = 0;


// --- INIT ---
// Initialises new data buffer. Capacity is provided in bytes.
// Returns false on error, otherwise true.
bool DataBuffer::init(uint32_t capacity) {
	if (buffer != 0) {
		// An existing buffer exists. Erase it first.
		delete[] buffer;
	}
	
	// Allocate new buffer and return result.
	buffer = new uint8_t[capacity];
	DataBuffer::capacity = capacity;
	
	end = buffer + capacity;
	front = buffer;
	back = buffer;
	index = buffer;
	
	size = 0;
	unread = 0;
	free = capacity;
	
	byteIndex = 0;
	byteIndexLow = 0;
	byteIndexHigh = 0;
	
	eof = false;
	dataRequestPending = false;
	seekRequestPending = false;
	state = DBS_IDLE;
	
	return true;
}


// --- CLEAN UP ---
// Clean up resources, delete the buffer.
bool DataBuffer::cleanup() {
	if (buffer != 0) {
		delete[] buffer;
		buffer = 0;
	}
	
#ifdef PROFILING_DB
	if (db_debugfile.is_open()) {
		db_debugfile.flush();
		db_debugfile.close();
	}
#endif
	
	return true;
}


// --- SET SEEK REQUEST CALLBACK ---
void DataBuffer::setSeekRequestCallback(SeekRequestCallback cb) {
	seekRequestCallback = cb;
}


// --- SET DATA REQUEST CONDITION ---
void DataBuffer::setDataRequestCondition(std::condition_variable* condition) {
	dataRequestCV = condition;
}


// -- SET SESSION HANDLE ---
void DataBuffer::setSessionHandle(uint32_t handle) {
	sessionHandle = handle;
}


// --- GET SESSION HANDLE ---
uint32_t DataBuffer::getSessionHandle() {
	return sessionHandle;
}


// --- SET FILE SIZE ---
void DataBuffer::setFileSize(int64_t size) {
	filesize = size;
}


// --- GET FILE SIZE ---
int64_t DataBuffer::getFileSize() {
	return filesize;
}


// --- START ---
// Starts calling the data request handler to obtain data.
bool DataBuffer::start() {
	if (dataRequestCV == 0) { return false; }
	
	dataRequestPending = true;
	dataRequestCV->notify_one();
	
	return true;
}


// --- REQUEST DATA ---
void DataBuffer::requestData() {
	if (dataRequestCV == 0) { return; }
	
	// Trigger a data request from the client.
	dataRequestPending = true;
	dataRequestCV->notify_one();
	
	// Wait until we have received data or time out.
	std::unique_lock<std::mutex> lk(dataWaitMutex);
	using namespace std::chrono_literals;
	while (dataWaitCV.wait_for(lk, 100us) != std::cv_status::timeout) {
		if (!dataRequestPending) { break; }
	}
}


// --- RESET ---
// Reset the buffer to the initialised state. This leaves the existing allocated buffer intact, 
// but erases its contents.
bool DataBuffer::reset() {
	front = buffer;
	back = buffer;
	size = 0;
	index = buffer;
	
	unread = 0;
	free = capacity;
	
	byteIndex = 0;
	byteIndexLow = 0;
	byteIndexHigh = 0;
	
	eof = false;
	dataRequestPending = false;
	seekRequestPending = false;
	state = DBS_IDLE;
	
	return true;
}


// --- SEEK ---
// Seek to a specific point in the data.
// Returns the new absolute byte position in the file, or -1 in case of failure.
int64_t DataBuffer::seek(DataBufferSeek mode, int64_t offset) {
#ifdef DEBUG
	std::cout << "DataBuffer::seek: mode " << mode << ", offset: " << offset << std::endl;
#endif
	
	// Calculate absolute byte index.
	int64_t new_offset = -1;
	if 		(mode == DB_SEEK_START)		{ new_offset = offset; }
	else if (mode == DB_SEEK_CURRENT) 	{ new_offset = byteIndex + offset; }
	else if (mode == DB_SEEK_END)		{ new_offset = filesize - offset - 1; }
	
#ifdef DEBUG
	std::cout << "New offset: " << new_offset << std::endl;
	std::cout << "ByteIndex: " << byteIndex << std::endl;
#endif

	// Ensure that the new offset isn't past the beginning/end of the file. If so, return -1.
	if (new_offset < 0 || new_offset > filesize) {
#ifdef DEBUG
		std::cout << "New offset larger than file size or negative. Returning -1." << std::endl;
#endif
		return -1;
	}
	
	// Ensure we're not in the midst of a data request action.
	uint32_t timeout = 1000;
	while (dataRequestPending) {
		// Sleep in 1 ms segments until the data request is done.
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (--timeout < 1) {
#ifdef DEBUG
		std::cout << "Seek time-out. Returning -1." << std::endl;
#endif
			// Seek failed, return -1.
			return -1;
		}
	}
	
#ifdef DEBUG
		std::cout << "Proceeding with reset." << std::endl;
#endif
	
	// We assume the local data isn't in the buffer and reload.
	reset();
	byteIndexLow = (uint32_t) new_offset;
	byteIndexHigh = (uint32_t) new_offset;
	if (seekRequestCallback == 0) { return -1; }
	seekRequestPending = true;
	state = DBS_SEEKING;
	seekRequestCallback(sessionHandle, new_offset);
	
	// Wait for response.
	std::unique_lock<std::mutex> lk(seekRequestMutex);
	using namespace std::chrono_literals;
	while (seekRequestPending) {
		std::cv_status stat = seekRequestCV.wait_for(lk, 1s);
		if (stat == std::cv_status::timeout) {
#ifdef DEBUG
			std::cout << "Time-out on seek request. Returning -1." << std::endl;
#endif
			return -1; 
		}
	}
		
	state = DBS_IDLE;
	
	byteIndex = (uint32_t) new_offset;
	
	return new_offset;
}


// --- SEEKING ---
bool DataBuffer::seeking() {
	return (state == DBS_SEEKING);
}


// --- READ ---
// Try to read 'len' bytes from the buffer, into the provided buffer.
// Returns the number of bytes read, or 0 in case of an error.
uint32_t DataBuffer::read(uint32_t len, uint8_t* bytes) {
#ifdef DEBUG
	std::cout << "DataBuffer::read: len " << len << ". EOF: " << eof << std::endl;
#endif

	// Request more data if the buffer does not have enough unread data left, and EOF condition
	// has not been reached.
	if (!eof && len > unread) {
		// More data should be available on the client, try to request it.
#ifdef DEBUG
		std::cout << "Requesting more data..." << std::endl;
#endif
		requestData();
	}
	
	if (unread == 0) {
		if (eof) {
#ifdef DEBUG
			std::cout << "Reached EOF." << std::endl;
#endif
			return 0;
		}
		else {
#ifdef DEBUG
			std::cout << "Read failed due to empty buffer." << std::endl;
#endif
			return 0;
		}
	}
	
	uint32_t bytesRead = 0;
	
	// Determine the number of bytes we can read in one copy operation.
	// This depends on the location of the write pointer ('back') compared to the 
	// read pointer ('index'). If the write pointer is ahead of the read pointer, we can read up 
	// till there, otherwise to the end of the buffer.
	uint32_t locunread = unread;
	uint32_t bytesSingleRead = locunread;
	if ((end - index) < bytesSingleRead) { bytesSingleRead = end - index; } // Unread section wraps around.
	
#ifdef DEBUG
	std::cout << "bytesSingleRead: " << bytesSingleRead << std::endl;
#endif
	
	if (len <= bytesSingleRead) {
		// Can read requested data in single chunk.
#ifdef DEBUG
		std::cout << "Read whole block." << std::endl;
		std::cout << "Index: " << index - buffer << ", Back: " << back - buffer << std::endl;
#endif
		memcpy(bytes, index, len);
		index += len;		// Advance read pointer.
		bytesRead += len;
		byteIndex += len;
		unread -= len;		// Unread bytes decreases by read byte count.
		free += len;		// Read bytes become free for overwriting.
		
		if (index >= end) {
			index = buffer;	// Read pointer went past the buffer end. Reset to buffer begin.
		}
	}
	else if (bytesSingleRead > 0 && locunread == bytesSingleRead) {
		// Less data in buffer than needed & nothing at the front.
		// Read what we can from the back, then return.
#ifdef DEBUG
		std::cout << "Read partial data from back." << std::endl;
		std::cout << "Index: " << index - buffer << ", Back: " << back - buffer << std::endl;
#endif
		memcpy(bytes, index, bytesSingleRead);
		index += bytesSingleRead;		// Advance read pointer.
		bytesRead += bytesSingleRead;
		byteIndex += bytesSingleRead;
		unread -= bytesSingleRead;		// Unread bytes decreases by read byte count.
		free += bytesSingleRead;		// Read bytes become free for overwriting.
		
		if (index >= end) {
			index = buffer;	// Read pointer went past the buffer end. Reset to buffer begin.
		}
	}
	else if (bytesSingleRead > 0 && locunread > bytesSingleRead) {
		// Read part from the end of the buffer, then read rest from the front.
#ifdef DEBUG
		std::cout << "Read back, then front." << std::endl;
		std::cout << "Index: " << index - buffer << ", Back: " << back - buffer << std::endl;
#endif
		memcpy(bytes, index, bytesSingleRead);
		index += bytesSingleRead;		// Advance read pointer.
		bytesRead += bytesSingleRead;
		byteIndex += bytesSingleRead;
		unread -= bytesSingleRead;		// Unread bytes decreases by read byte count.
		free += bytesSingleRead;		// Read bytes become free for overwriting.
		
		index = buffer;	// Switch read pointer to front of the buffer.
		
		// Read remainder from front.
		uint32_t bytesToRead = len - bytesRead;
#ifdef DEBUG
		std::cout << "bytesRead: " << bytesRead << ", bytesToRead: " << bytesToRead << std::endl;
#endif
		if (bytesToRead <= locunread) {
			// Read the remaining bytes we need.
			memcpy(bytes + bytesRead, index, bytesToRead);
			index += bytesToRead;
			bytesRead += bytesToRead;
			byteIndex += bytesToRead;
			unread -= bytesToRead;
			free += bytesToRead;
		}
		else {
			// Read the unread bytes still available in the buffer.
			memcpy(bytes + bytesRead, index, locunread);
			index += locunread;
			bytesRead += locunread;
			byteIndex += locunread;
			unread -= locunread;
			free += locunread;
		}
	}
	else {
		// Default case.
#ifdef DEBUG
		std::cout << "FIXME: Default case." << std::endl;
#endif
		// FIXME: This shouldn't happen.
		
	}
	
	// Trigger a data request from the client if we have space.
	if (eof) {
		// Do nothing.
	}
	else if (!dataRequestPending && free > 204799) {
		// Single block is 200 kB (204,800 bytes). We have space, so request another block.
		// TODO: make it possible to request a specific block size from client.
		if (dataRequestCV != 0) {
			dataRequestPending = true;
			dataRequestCV->notify_one();
		}
	}
	
#ifdef DEBUG
	std::cout << "unread " << unread << ", free " << free << std::endl;
	std::cout << "bytesRead: " << bytesRead << std::endl;
#endif
	
	return bytesRead;
}


// --- WRITE ---
// Write data into the buffer.
uint32_t DataBuffer::write(std::string &data) {
	return write(data.data(), data.length());
}


uint32_t DataBuffer::write(const char* data, uint32_t length) {
#ifdef DEBUG
	std::cout << "DataBuffer::write: len " << length << std::endl;
	std::cout << "Index: " << index - buffer << ", Back: " << back - buffer << std::endl;
#endif

	// First check whether we can perform a straight copy. For this we need enough available bytes
	// at the end of the buffer. Else we have to attempt to write the remainder into the front of
	// the buffer.
	// The bytesFreeLow and bytesFreeHigh counters are for keeping track of the number of free bytes
	// at the low (beginning) and high (end) side respectively.
	uint32_t bytesWritten = 0;
	
	// Determine the number of bytes we can write in one copy operation.
	// This depends on the number of 'free' bytes, and the location of the read pointer ('index') 
	// compared to the  write pointer ('back'). If the read pointer is ahead of the write pointer, 
	// we can write up till there, otherwise to the end of the buffer.
	uint32_t locfree = free;
	uint32_t bytesSingleWrite = locfree;
	if ((end - back) < bytesSingleWrite) { bytesSingleWrite = end - back; }
	
	if (length <= bytesSingleWrite) {
#ifdef DEBUG
		std::cout << "Write whole chunk at back. Single write: " << bytesSingleWrite << std::endl;
#endif
		// Enough space to write the data in one go.
		memcpy(back, data, length);
		bytesWritten = length;
		back += bytesWritten;
		unread += bytesWritten;
		free -= bytesWritten;
		
		if (back >= end) {
			back = buffer;
		}
	}
	else if (bytesSingleWrite > 0 && locfree == bytesSingleWrite) {
#ifdef DEBUG
		std::cout << "Partial write at back. Single write: " << bytesSingleWrite << std::endl;
#endif
		// Only enough space in buffer to write to the back. Write what we can, then return.
		memcpy(back, data, bytesSingleWrite);
		bytesWritten = bytesSingleWrite;
		back += bytesWritten;
		unread += bytesWritten;
		free -= bytesWritten;
		
		if (back >= end) {
			back = buffer;
		}
	}
	else if (bytesSingleWrite > 0 && locfree > bytesSingleWrite) {
#ifdef DEBUG
		std::cout << "Partial write at back, rest at front. Single write: " << bytesSingleWrite << std::endl;
#endif
		// Write to the back, then the rest at the front.
		memcpy(back, data, bytesSingleWrite);
		bytesWritten = bytesSingleWrite;
		unread += bytesWritten;
		free -= bytesWritten;
		locfree -= bytesWritten;
		
		back = buffer;
		
		// Write remainder at the front.
		uint32_t bytesToWrite = length - bytesWritten;
#ifdef DEBUG
	std::cout << "Write remainder: " << bytesToWrite << std::endl;
	std::cout << "Index: " << index - buffer << ", Back: " << back - buffer << std::endl;
#endif
		if (bytesToWrite <= locfree) {
			// Write the remaining bytes we have.
			memcpy(back, data + bytesWritten, bytesToWrite);
			bytesWritten += bytesToWrite;
			unread += bytesToWrite;
			free -= bytesToWrite;
			back += bytesToWrite;
		}
		else {
			// Write the unread bytes still available in the buffer.
			memcpy(back, data + bytesWritten, locfree);
			bytesWritten += locfree;
			unread += locfree;
			free -= locfree;
			back += locfree;
		}
	}
	else {
		// FIXME: shouldn't happen.
#ifdef DEBUG
		std::cout << "FIXME: Default case." << std::endl;
#endif
	}
	
#ifdef DEBUG
		std::cout << "unread: " << unread << ", free: " 
					<< free << ", bytesWritten: " << bytesWritten << std::endl;
#endif
	
	// If we're in seeking mode, signal that we're done.
	if (state == DBS_SEEKING) {
#ifdef DEBUG
		std::cout << "In seeking mode. Notifying seeking routine." << std::endl;
#endif
		seekRequestPending = false;
		dataRequestPending = false;
		seekRequestCV.notify_one();
		
		return bytesWritten;
	}
	
#ifdef DEBUG
		std::cout << "Data request done." << std::endl;
#endif
	
	dataRequestPending = false;
	
	// Trigger a data request from the client if we have space.
	if (eof) {
		// Do nothing.
	}
	else if (free > 204799) {
#ifdef DEBUG
		std::cout << "Data request started." << std::endl;
#endif
		// Single block is 200 kB (204,800 bytes). We have space, so request another block.
		// TODO: make it possible to request a specific block size from client.
		if (dataRequestCV != 0) {
			dataRequestPending = true;
			dataRequestCV->notify_one();
		}
	}
	
	return bytesWritten;
}


// --- SET EOF ---
// Set the End-Of-File status of the file being streamed.
void DataBuffer::setEof(bool eof) {
	DataBuffer::eof = eof;
}


// --- IS EOF ---
bool DataBuffer::isEof() {
	return DataBuffer::eof;
}
