-- test_databuffer.adb - Test for the lock-free ring buffer.

-- Features:
--			- Simple buffer test.


with LFRingDataBuffer;

with Ada.Text_IO; use Ada.Text_IO;
with Interfaces; use Interfaces;
with Ada.Characters.Latin_1; use Ada.Characters.Latin_1;


with dataRequest; use dataRequest;


procedure test_databuffer is
	package DB renames LFRingDataBuffer;
	bytes 		: DB.buff_array (0..7);
	expected 	: Unsigned_8 	:= 0;
	aborted		: Boolean 		:= False;
	read		: Unsigned_32;
	drq			: DB.drq_access;
	initret		: Boolean;
	idx			: Unsigned_32 := 0;
	emptied		: Boolean;
begin
	put_line("Running DataBuffer test...");
	
	-- Create 20 byte buffer.
	initret := DB.init(20);
	if initret = False then
		put_line("DB Init failed.");
		return;
	end if;
	
	-- Start and set data request task.
	drq := new dataRequestTask;
	DB.setDataRequestTask(drq);
	
	-- The test file data is 100 bytes. We read 10 byte chunks.
	emptied := False;
	while emptied = False loop
		read := DB.read(8, bytes);
		if read = 0 then
			put_line("Failed to read. Aborting.");
			exit;
		end if;
		
		idx := 0;
		for i in 0 .. Integer(read - 1) loop
			put(bytes(idx)'Image & " ");
			if expected /= bytes(idx) then
				aborted := True;
			end if;
			
			idx		:= idx + 1;
			expected := expected + 1;
		end loop;
		
		put_line("");
		
		if aborted = True then
			put_line("Detected mismatch. Aborting read...");
			exit;
		end if;
		
		if DB.isEoF then
			emptied := DB.isEmpty;
		end if;
	end loop;
	
	put(LF & "Test results: ");
	if aborted = True then
		put_line("Failed.");
	else
		put_line("Success.");
	end if;
	
	put_line(LF & "Finished test. Shutting down...");
end test_databuffer;

