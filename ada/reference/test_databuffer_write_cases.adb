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

-- Program's entry point.

procedure test_databuffer_write_cases is
	package DB renames LFRingDataBuffer;

-- Return value as byte type.

function to_byte(value: Integer) return Unsigned_8 is
begin
   return Unsigned_8(value);
end to_byte;

-- Return value as index type.

function to_index(value: Integer) return Unsigned_32 is
begin
   return Unsigned_32(value);
end to_index;

-- Fill data monotonically increasing, starting at `start`, return next value to use.

function fill(data: in out DB.buff_array; start: Integer) return Integer is
begin
	for i in 0 .. data'Length - 1 loop
		data(to_index(i)) := to_byte(i + start);
	end loop;
	return start + data'Length;
end fill;

-- Return True if data is increasing monotonically.

function is_monotonic(data: DB.buff_array) return Boolean is
begin
	-- prevent out-of-range indexing.
	if data'Length <= 1 then
		return True;
	end if;

	for i in 0 .. data'Length - 2 loop
		if data(to_index(i + 1)) - data(to_index(i)) /= 1 then
			return False;
		end if;
	end loop;
	return True;
end is_monotonic;

-- Return "monotonic", or "non-monotonic", depending on data monotonicity.

function str_monotonic(data: DB.buff_array) return String is
begin
	if is_monotonic(data) then
		return "monotonic";
	else
		return "non-monotonic";
	end if;
end str_monotonic;

-- Report the content read, return True if it is monotonically increasing.

function report(title: String; data: DB.buff_array) return Boolean is
begin
	put(title & ": [");
	for i in 0 .. data'Length - 1 loop
		put(Unsigned_8'Image(data(to_index(i))) & ",");
	end loop;
	put_line("]: " & str_monotonic(data));
	return is_monotonic(data);
end report;

-- Separate read to use in non-zero-sized reads (prevent creating zero-sized read buffers).
-- Report the content read, return True if it is monotonically increasing.

function read(Nread: Unsigned_32) return Boolean is
	Bread: DB.buff_array (0..Nread - 1);		-- at least 1 byte
begin
	Assert(Nread = DB.read(Nread, Bread));
	return report("Bread", Bread);
end read;

-- Test write, read sequence specified bij buffer capacity, prefill (warmup), partial read and write.

procedure test_databuffer_case(title: String; capacity: Unsigned_32; Nwarmup: Unsigned_32; Nread: Unsigned_32; Nwrite: Unsigned_32; note: String) is
	Bwarmup: DB.buff_array (0..Nwarmup - 1);
	Bwrite : DB.buff_array (0..Nwrite - 1);
	Nactual: Unsigned_32 := 0;
	value  : Integer := 0;
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

	-- Fill buffers with monotonically increasing data to write to the ring buffer.
	value := fill(Bwarmup, value);
	value := fill(BWrite, value);

	Assert(report("Bwarmup", Bwarmup));			-- Assert monotonicity
	Assert(report("Bwrite", Bwrite));			-- Assert monotonicity
	
	Assert(Nwarmup = DB.write(Bwarmup));
	if Nread > 0 then
		Assert(read(Nread));					-- Assert monotonicity
	end if;
	Nactual := DB.write(Bwrite);
	Assert(Nactual <= Nwrite);					-- Not all may fit in ring buffer.
	Assert(read(Nwarmup - Nread + Nactual));	-- Read all what's in ring buffer, asserting monotonicity.
end test_databuffer_case;

-- Main program.

begin
	--								Ring buffer size
	--                              |  Nwarmup
	--                              |  |  Nread
	--                              |  |  |  Nwrite
	test_databuffer_case("Test 1:", 7, 3, 0, 3, "Single write at back");				-- branch: if data'Length <= bytesSingleWrite then
	test_databuffer_case("Test 2:", 7, 4, 0, 4, "Single write at back, only part");		-- branch: elsif bytesSingleWrite > 0 and locfree = bytesSingleWrite then
	test_databuffer_case("Test 3:", 7, 5, 3, 5, "Partly write at back, rest at front");	-- branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
	test_databuffer_case("Test 4:", 7, 5, 3, 6, "Partly write at back, part at front");	-- branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
end test_databuffer_write_cases;
