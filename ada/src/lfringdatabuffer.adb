-- databuffer.adb - Implementation of the lock-free ring buffer.

-- 2024/02/29 - Maya Posch

with Ada.Unchecked_Deallocation;

with Ada.Text_IO; use Ada.Text_IO;


package body LFRingDataBuffer is
	-- implementation
	procedure free_buffer is new Ada.Unchecked_Deallocation
			(Object => buff_array, Name => buff_ref);
	
	-- INIT --
	function init(capacity: Unsigned_32) return Boolean is
	begin
		-- Check for an existing buffer. Delete if found.
		if (buffer /= null) then
			-- An existing buffer exists. Erase it first.
			free_buffer (buffer);
		end if;
		
		buffer := new buff_array(0 .. capacity - 1);
		LFRingDataBuffer.capacity := capacity;
		
		size 	:= 0;
		unread 	:= 0;
		free 	:= capacity;
		
		buff_last 	:= capacity - 1;
		data_front	:= 0;
		data_back	:= 0;
		read_index	:= 0;
		
		byteIndex := 0;
		byteIndexLow := 0;
		byteIndexHigh := 0;
		
		eof := false;
		state := DBS_IDLE;
		
		return true;
	end init;
	
	
	-- CLEAN UP --
	function cleanup return Boolean is
	begin
		if (buffer /= null) then
			-- delete an existing buffer.
			free_buffer(buffer);
			buffer := null;
		end if;
		
		return true;
	end cleanup;
	
	
	-- SET SEEK REQUEST CALLBACK --
	procedure setSeekRequestCallback(cb: seekRequestCB) is
	begin
		seekCB := cb;
	end setSeekRequestCallback;
	
	
	-- SET DATA REQUEST TASK --
	procedure setDataRequestTask(dt: drq_access) is
	begin
		readT := dt;
	end setDataRequestTask;
	
	
	-- SET SESSION HANDLE ---
	procedure setSessionHandle(handle: Unsigned_32) is
	begin
		--
		null;
	end setSessionHandle;
	
	
	function getSessionHandle return Unsigned_32 is
	begin
		--
		return 0;
	end getSessionHandle;
	
	
	procedure setFileSize(size: Integer_64) is
	begin
		--
		null;
	end setFileSize;
	
	
	function getFileSize return Integer_64 is
	begin
		--
		return 0;
	end getFileSize;
	
	
	function start return Boolean is
	begin
		--
		return False;
	end start;
	
	
	-- REQUEST DATA --
	procedure requestData is
	begin
		-- Trigger data request from the client.
		dataRequestPending := True;
		readT.fetch;
		
		-- Wait until we have received data or time out.
		while dataRequestPending = True loop
			delay 0.1; -- delay 100 ms.
			put_line("Fetching...");
		end loop;
		
		put_line("Fetched data.");
	end requestData;
	
	
	function reset return Boolean is
	begin
		--
		return False;
	end reset;
	
	
	function seek(mode: DataBufferSeek; offset: Integer_64) return Integer_64 is
	begin
		--
		return 0;
	end seek;
	
	
	function seeking return Boolean is
	begin
		--
		return False;
	end seeking;
	
	
	function read(len: Unsigned_32; bytes: in out buff_array) return Unsigned_32 is
		bytesRead	: Unsigned_32;
		locunread	: Unsigned_32;
		bytesSingleRead	: Unsigned_32;
		bytesToRead	: Unsigned_32;
		tidx		: Unsigned_32;
		readback	: Unsigned_32;	-- Last index to read (constraint).
	begin
		-- Request more data if the buffer does not have enough unread data left, and EOF condition
		-- has not been reached.
		if eof = false and len > unread then
			put_line("Requesting data...");
			requestData;
		end if;
		
		if unread = 0 then
			if eof = True then
				-- Reached EOF.
				put_line("Reached EOF.");
				return 0;
			else
				-- Read failed due to empty buffer.
				put_line("Read failed due to empty buffer.");
				return 0;
			end if;
		end if;
			
		bytesRead := 0;
			
		-- Determine the number of bytes we can read in one copy operation.
		-- This depends on the location of the write pointer ('data_back') compared to the
		-- read pointer ('read_index). If the write pointer is ahead of the read pointer, we can
		-- read up till there, otherwise to the end of the buffer.
		locunread := unread;
		bytesSingleRead := locunread;
		if (buff_last - read_index) < bytesSingleRead then
			-- Unread section wraps around.
			bytesSingleRead := buff_last - read_index + 1;
		end if;
			
		if len <= bytesSingleRead then
			-- Can read requested data in single chunk.
			readback	:= (read_index + len) - 1;
			bytes 		:= buffer.all(read_index .. readback);
			read_index 	:= read_index + len; 	-- Advance read index.
			bytesRead 	:= bytesRead + len;
			byteIndex	:= byteIndex + len;
			unread		:= unread - len;		-- Unread bytes decrease by read byte count.
			free		:= free + len;			-- Read bytes become free for overwriting.
			
			if read_index >= buff_last then
				read_index := 0; -- Read index went past buffer end. Reset to buffer begin.
			end if;
		elsif bytesSingleRead > 0 and locunread = bytesSingleRead then
			-- Less data in buffer than needed & nothing at the front.
			-- Read what we can from the back, then return.
			readback	:= (read_index + bytesSingleRead) - 1;
			tidx		:= bytesSingleRead - 1;
			bytes(0 .. tidx)	:= buffer.all(read_index .. readback);
			read_index	:= read_index + bytesSingleRead;	-- Advanced read index.
			bytesRead	:= bytesRead + bytesSingleRead;
			byteIndex	:= byteIndex + bytesSingleRead;
			unread		:= unread - bytesSingleRead;		-- Unread bytes decrease by read count.
			free		:= free + bytesSingleRead;			-- Read bytes become free bytes.
			
			if read_index >= buff_last then
				read_index := 0; -- Read index went past buffer end. Reset to buffer begin.
			end if;
		elsif bytesSingleRead > 0 and locunread > bytesSingleRead then
			-- Read part from the end of the buffer, then read rest from the front.
			readback	:= (read_index + bytesSingleRead) - 1;
			tidx		:= bytesSingleRead - 1;
			bytes(0 .. tidx)	:= buffer.all(read_index .. readback);
			bytesRead	:= bytesRead + bytesSingleRead;
			byteIndex	:= byteIndex + bytesSingleRead;
			unread		:= unread - bytesSingleRead;
			free		:= free + bytesSingleRead;
			
			read_index	:= 0;	-- Switch index to front of the buffer.
			
			-- Read remainder from front.
			bytesToRead	:= len - bytesRead;
			if bytesToRead <= locunread then
				-- Read the remaining bytes.
				readback	:= (read_index + bytesToRead) - 1;
				tidx 		:= bytesSingleRead + (bytesToRead - 1);
				bytes(bytesSingleRead .. tidx) := buffer.all(read_index .. readback);
				read_index	:= read_index + bytesToRead;
				bytesRead	:= bytesRead + bytesToRead;
				byteIndex	:= byteIndex + bytesToRead;
				unread		:= unread - bytesToRead;
				free		:= free + bytesToRead;
			else
				-- Read the remaining bytes still available in the buffer.
				readback	:= (read_index + locunread) - 1;
				tidx		:= bytesSingleRead + (locunread - 1);
				bytes(bytesSingleRead .. tidx) := buffer.all(read_index .. readback);
				read_index	:= read_index + locunread;
				bytesRead	:= bytesRead + locunread;
				byteIndex	:= byteIndex + locunread;
				unread		:= unread - locunread;
				free		:= free + locunread;
			end if;
		end if;
		
		if eof = True then
			-- Do nothing
			null;
		elsif dataRequestPending = False and free > 204799 then
			-- Single block is 200 kB (204,800 bytes). We have space, so request another block.
			-- TODO: make it possible to request a specific block size from client.
			if readT /= null then
				dataRequestPending := True;
				readT.fetch;	-- Prod fetch entry in data request handler.
			end if;
		end if;
		
		return bytesRead;
	end read;
	
	
	-- WRITE --
	function write(data: buff_array) return Unsigned_32 is
		bytesWritten	: Unsigned_32 	:= 0;
		locfree			: Unsigned_32;
		bytesSingleWrite: Unsigned_32;
		bytesToWrite	: Unsigned_32;
		writeback		: Unsigned_32;
	begin
		-- First check whether we can perform a straight copy. For this we need enough available
		-- bytes at the end of the buffer. Else we have to attempt to write the remainder into the
		-- front of the buffer.
		-- The bytesFreeLow and bytesFreeHigh counters are for keeping track of the number of free
		-- bytes at the low (beginning) and high (end) side respectively.
		
		-- Determine the number of bytes we can write in one copy operation.
		--This depends on the number of 'free' bytes, and the location of the read pointer
		-- ('read_index') compared to the write pointer ('data_back'). If the read pointer is ahead of 
		-- the write pointer, we can write up till there, otherwise to the end of the buffer.
		locfree := free;
		bytesSingleWrite := free;
		if (buff_last - data_back) < bytesSingleWrite then
			bytesSingleWrite := buff_last - data_back + 1;
		end if;
		
		put_line("Writing: " & Unsigned_8'Image(data'Length) & " bytes.");
		put_line("BytesSingleWrite: " & Unsigned_32'Image(bytesSingleWrite));
		put_line("locfree: " & Unsigned_32'Image(locfree));
		put_line("free: " & Unsigned_32'Image(free));
		put_line("capacity: " & Unsigned_32'Image(capacity));
		put_line("data_back: " & Unsigned_32'Image(data_back));
		
		if data'Length <= bytesSingleWrite then
			-- Enough space to write the data in one go.
			put_line("Write whole chunk at back.");
			writeback		:= (data_back + data'Length) - 1;
			buffer.all(data_back .. writeback) := data;
			bytesWritten	:= data'Length;
			data_back		:= data_back + bytesWritten;
			unread			:= unread + bytesWritten;
			free			:= free - bytesWritten;
		
			if data_back >= buff_last then
				data_back := 0;
			end if;
		elsif bytesSingleWrite > 0 and locfree = bytesSingleWrite then
			-- Only enough space in buffer to write to the back. Write what we can, then return.
			put_line("Partial write at back.");
			writeback		:= (data_back + bytesSingleWrite) - 1;
			buffer.all(data_back .. writeback) := data(0 .. bytesSingleWrite - 1);
			bytesWritten	:= bytesSingleWrite;
			data_back		:= data_back + bytesWritten;
			unread			:= unread + bytesWritten;
			free			:= free - bytesWritten;
			
			if data_back >= buff_last then
				data_back := 0;
			end if;
		elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
			-- Write to the back, then the rest at the front.
			put("Partial write at back, ");
			writeback		:= (data_back + bytesSingleWrite) - 1;
			buffer.all(data_back .. writeback) := data(0 .. bytesSingleWrite - 1);
			bytesWritten	:= bytesSingleWrite;
			unread			:= unread + bytesWritten;
			free			:= free - bytesWritten;
			
			data_back := 0;
			
			-- Write remainder at the front.
			bytesToWrite := data'Length - bytesWritten;
			if bytesToWrite <= locfree then
				-- Write the remaining bytes we have.
				put_line("rest at front.");
				writeback		:= (data_back + bytesToWrite) - 1;
				buffer.all(data_back .. writeback) := data(bytesWritten .. data'Last);
				bytesWritten	:= bytesWritten + bytesToWrite;
				unread			:= unread + bytesToWrite;
				free			:= free - bytesToWrite;
				data_back		:= data_back + bytesToWrite;
			else
				-- Write the remaining space still available in the buffer.
				put_line("part at front.");
				writeback		:= (data_back + locfree) - 1;
				buffer.all(data_back .. writeback) := data(bytesWritten - 1 .. locfree);
				bytesWritten	:= bytesWritten + locfree;
				unread			:= unread + locfree;
				free			:= free - locfree;
				data_back		:= data_back + locfree;
			end if;
		else
			-- If we get here, the universe just exploded.
			null;
		end if;
		
		-- If we're in seeking mode, signal that we're done.
		if state = DBS_SEEKING then
			seekRequestPending := False;
			dataRequestPending := False;
			--seekCB(session, ; -- Seeking callback.
			-- TODO: Implement seek stuff.
			
			return bytesWritten;
		end if;
		
		dataRequestPending := False;
		
		-- Trigger a data request from the client if we have space.
		if eof = true then
			-- Do nothing.
			null;
		elsif free > 204799 then
			-- Request another block.
			if readT /= null then
				readT.fetch;
			end if;
		end if;
		
		return bytesWritten;
	end write;
	
	
	-- SET EOF --
	procedure setEof(eof: Boolean) is
	begin
		LFRingDataBuffer.eof := eof; 
	end setEof;
	
	
	-- IS EOF --
	function isEof return Boolean is
	begin
		return eof;
	end isEof;
	
	-- IS EMPTY --
	function isEmpty return Boolean is
	begin
		if unread > 0 then
			return False;
		else
			return True;
		end if;
	end isEmpty;
	
end LFRingDataBuffer;