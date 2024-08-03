-- test_databuffer-write-cases.adb - Test for the lock-free ring buffer.

-- Features:
-- Test: 3 write cases:
-- - Enough space to write the data in one go.
-- - Only enough space in buffer to write to the back. Write what we can, then return.
-- - Write to the back, then the rest at the front, two subcases for writing at front.


with LFRingDataBuffer;

with Ada.Assertions; use Ada.Assertions;
with Ada.Text_IO; use Ada.Text_IO;
with Interfaces; use Interfaces;
with Ada.Characters.Latin_1; use Ada.Characters.Latin_1;


with dataRequest; use dataRequest;

procedure test_databuffer_write_cases is
	package DB renames LFRingDataBuffer;

-- TODO: define and check data element content.

procedure fill(data: DB.buff_array) is
begin
	for i in 0 .. data'Length loop
		null;
	end loop;
end fill;

-- Separate read to use in non-zero-sized reads (prevent creating zero-sized read buffers).

procedure assert_read(Nread: Unsigned_32) is
	Bread: DB.buff_array (0..Nread - 1);		-- at least 1 byte
begin
	Assert(Nread = DB.read(Nread, Bread));
end assert_read;

-- Test write, read sequence specified bij buffer capacity, prefill (warmup), partial read and write.

procedure test_databuffer_case(title: String; capacity: Unsigned_32; Nwarmup: Unsigned_32; Nread: Unsigned_32; Nwrite: Unsigned_32; note: String) is
	Bwarmup: DB.buff_array (0..Nwarmup - 1);
	Bwrite : DB.buff_array (0..Nwrite - 1);
begin
	put_line(
		LF & title   &
		" capacity:" & Unsigned_32'Image(capacity) &
		" warmup:"   & Unsigned_32'Image(Nwarmup)  &
		" read:"     & Unsigned_32'Image(Nread)    &
		" write:"    & Unsigned_32'Image(Nwrite)   &
		" - "        & note
	);

	-- Create ring buffer.
	if not DB.init(capacity) then
		put_line("DB Init failed.");
		return;
	end if;
	
	Assert(Nwarmup = DB.write(Bwarmup));
	if Nread > 0 then
		assert_read(NRead);
	end if;
	Assert(Nwrite >= DB.write(Bwrite));	-- may be less
end test_databuffer_case;

-- Main program.

begin
	test_databuffer_case("Test 1:", 7, 3, 0, 3, "Single write at back");				-- branch: if data'Length <= bytesSingleWrite then
	test_databuffer_case("Test 2:", 7, 4, 0, 4, "Single write at back, only part");		-- branch: elsif bytesSingleWrite > 0 and locfree = bytesSingleWrite then
	test_databuffer_case("Test 3:", 7, 5, 3, 5, "Partly write at back, rest at front");	-- branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
	test_databuffer_case("Test 4:", 7, 5, 3, 8, "Partly write at back, part at front");	-- branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
end test_databuffer_write_cases;
