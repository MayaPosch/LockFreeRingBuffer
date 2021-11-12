/*
	ffplaydummy.cpp - Implementation file for the dummy ffplay class.
	
	Revision 0
	
	Notes:
			- 
		
	2021/09/29 - Maya Posch
*/


#include "readdummy.h"

#include <thread>
#include <condition_variable>
#include <mutex>


// Static definitions.
ChronoTrigger ReadDummy::ct;
//int ReadDummy::buf_size = 32 * 1024;
int ReadDummy::buf_size = 128 * 1024;
uint8_t* ReadDummy::buf;
uint8_t ReadDummy::count = 0;

// Globals
uint32_t start_size = 27 * 1024; 	// Initial size of a request.
uint32_t step_size = 1 * 1014;		// Request 1 kB more each cycle until buf_size is reached.
uint32_t read_size = 0;


#define AVSEEK_SIZE 200


// Global objects.
std::condition_variable dummyCon;
std::mutex dummyMutex;
// ---


#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string>
#include <cstring>
#include <vector>
#include <chrono>

#include "databuffer.h"


/**
 * Reads from a buffer into provided buffer.
 *
 * @param ptr	   A pointer to the user-defined IO data structure.
 * @param buf	   A buffer to read into.
 * @param buf_size  The size of the buffer buff.
 *
 * @return The number of bytes read into the buffer.
 */
int ReadDummy::media_read(void* opaque, uint8_t* buf, int buf_size) {
	uint32_t bytesRead = DataBuffer::read(buf_size, buf);
	std::cout << "Read " << bytesRead << " bytes." << std::endl;
	if (bytesRead == 0) {
		std::cout << "EOF is " << DataBuffer::isEof() << std::endl;
		if (DataBuffer::isEof()) { return -1; }
		else { return -1; }
	}
	
	return bytesRead;
}


/**
 * Seeks to a given position in the currently open file.
 * 
 * @param opaque  A pointer to the user-defined IO data structure.
 * @param offset  The position to seek to.
 * @param whence  SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE.
 *
 * @return  The new byte position in the file or -1 in case of failure.
 */
int64_t ReadDummy::media_seek(void* opaque, int64_t offset, int whence) {
	std::cout << "media_seek: offset " << offset << ", whence " << whence << std::endl;
							
	int64_t new_offset = -1;
	switch (whence) {
		case SEEK_SET:	// Seek from the beginning of the file.
			std::cout << "media_seek: SEEK_SET" << std::endl;
			new_offset = DataBuffer::seek(DB_SEEK_START, offset);
			break;
		case SEEK_CUR:	// Seek from the current position.
			std::cout << "media_seek: SEEK_CUR" << std::endl;;
			new_offset = DataBuffer::seek(DB_SEEK_CURRENT, offset);
			break;
		case SEEK_END:	// Seek from the end of the file.
			std::cout << "media_seek: SEEK_END" << std::endl;
			new_offset = DataBuffer::seek(DB_SEEK_END, offset);
			break;
		case AVSEEK_SIZE:
			std::cout << "media_seek: received AVSEEK_SIZE, returning file size." << std::endl;
			return DataBuffer::getFileSize();
			break;
		default:
			std::cout << "media_seek: default. The universe broke." << std::endl;
	}
	
	if (new_offset < 0) {
		std::cout << "Error during seeking." << std::endl;
		new_offset = -1;
	}
	
	std::cout << "New offset: " << new_offset << std::endl;
	
	return new_offset;
}


// --- TRIGGER READ ---
void ReadDummy::triggerRead(int) {
	// TODO: if first read, seek to end - N bytes.
	// If second read, seek to beginning.
	if (count == 0) {
		// Seek to end - 10 kB.
		media_seek(0, DataBuffer::getFileSize() - (10 * 1024), SEEK_SET);
		count++;
		return;
	}
	else if (count == 1) {
		// Seek to beginning.
		media_seek(0, 0, SEEK_SET);
		count++;
		return;
	}
	
	// Call read() with a data request. This data is then discarded.
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	read_size += step_size;
	if (read_size > buf_size) {
		read_size = start_size;
	}
	
	if (media_read(0, buf, read_size) == -1) {
		// Signal the player thread that the playback has ended.
		dummyCon.notify_one();
	}
	
	// Report time for the media_read call.
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Read duration: " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "µs." << std::endl;
	
}


void ReadDummy::cleanUp() {
	//
}


// --- RUN ---
void ReadDummy::run() {
	buf = (uint8_t*) malloc(buf_size);
	
	read_size = start_size;
	
	// Start dummy playback:
	// - Request a 32 kB data block every N milliseconds.
	// - Once EOF is returned, wait N milliseconds, then quit.
	ct.setCallback(ReadDummy::triggerRead, 0);
	ct.setStopCallback(ReadDummy::cleanUp);
	ct.start(50);	// Trigger time in milliseconds.
	
	// Wait here until finished.
	std::unique_lock<std::mutex> lk(dummyMutex);
	dummyCon.wait(lk);
	
	ct.stop();
	
	DataBuffer::reset();	// Clears the data buffer (file data buffer).
	
	// Clean up.
	free(buf);
}

 
// --- QUIT ---
void ReadDummy::quit() {
	dummyCon.notify_one();
}
