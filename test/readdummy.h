/*
	readdummy.h - Header file for the dummy read class.
	
	Revision 0
	
	Notes:
			- 
		
	2021/09/29 - Maya Posch
*/


#ifndef READDUMMY_H
#define READDUMMY_H


#include <iostream>
#include <atomic>
#include <queue>

#include "chronotrigger.h"

	
class ReadDummy {
	static ChronoTrigger ct;
	static int buf_size;
	static uint8_t* buf;
	static uint8_t count;
	
	static void triggerRead(int);
	static void cleanUp();
	static int media_read(void* opaque, uint8_t* buf, int buf_size);
	static int64_t media_seek(void* opaque, int64_t pos, int whence);
	
public:
	static void run();
	static void quit();
};


#endif
