

with LFRingDataBuffer;

with Ada.Text_IO; use Ada.Text_IO;
with Interfaces; use Interfaces;
with Ada.Characters.Latin_1; use Ada.Characters.Latin_1;


package body dataRequest is
	package DB renames LFRingDataBuffer;
	
	task body dataRequestTask is
		lastnum : Unsigned_8 := 0;
		--data	: String(0 .. 9);
		data	: DB.buff_array (0..9);
		wrote	: Unsigned_32;
		idx		: Unsigned_32 := 0;
	begin
		put_line("Entering data request loop...");
		loop
			select
				accept fetch do
					-- Write into the buffer. We use a 10 byte pattern.
					idx	:= 0;
					for i in 0 .. 9 loop
						--data(i) := Character'Val(lastnum);
						data(idx) := lastnum;
						lastnum := lastnum + 1;
						idx		:= idx + 1;
					end loop;
					
					wrote := DB.write(data);
					
					put_line("Wrote " & Unsigned_32'Image(wrote) & HT & "- ");
					idx := 0;
					for i in 0 .. Integer(wrote - 1) loop
						put(Unsigned_8'Image(data(idx)) & " ");
						idx := idx + 1;
					end loop;
					
					put(LF);
					
					if lastnum >= 100 then
						DB.setEoF(True);
					end if;
				end fetch;
			or
				terminate;
			end select;
		end loop;
	end dataRequestTask;
end DataRequest;
	