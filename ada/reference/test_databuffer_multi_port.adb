-- test_databuffer_multi_port.adb - Test for the lock-free ring buffer.

-- Features:
--			- Multi-port test with separate reader and writer.


with Interfaces; use Interfaces;


-- Globals
chunk_size : Unsigned_32 := 200 * 1024;
type chunk_array is array(Integer range <>) of Unsigned_8;
type chunk_ref is access chunk_array;
chunk : chunk_ref;


--- DATA REQUEST FUNCTION ---
-- This function can be signalled with the condition variable to request data from the client.
procedure dataRequestFunction is
--
begin
	--
end dataRequestFunction;


--- WRITE BUFFER ---
-- Write data into the buffer when signalled.
procedure dataWriteFunction is
--
begin
	--
end dataWriteFunction;


--- SEEKING HANDLER ---
procedure seekingHandler(session: in Integer; offset: in Integer) is
--
begin
	--
end seekingHandler;


procedure signal_handler(signal: Integer) is
--
begin
	--
end signal_handler;


procedure test_databuffer_multi_port is
--
begin
	-- Set up buffer.
	chunk := new 
end test_databuffer_multi_port;
