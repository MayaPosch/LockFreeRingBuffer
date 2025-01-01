-- databuffer.ads - Specification for the lock-free ring buffer.

-- Revision 0

-- 2024/02/29 - Maya Posch

with Interfaces; use Interfaces;
with dataRequest; use dataRequest;
with System.Atomic_Operations.Integer_Arithmetic;
with System.Atomic_Operations.Exchange;


package LFRingDataBuffer is
	-- public
	type DataBufferSeek is
		(DB_SEEK_START,
		DB_SEEK_CURRENT,
		DB_SEEK_END);
		
	type buff_array is array(Unsigned_32 range <>) of Unsigned_8;
	--type seekRequestCB is not null access procedure (session: Integer; offset: Unsigned_64);
	type seekRequestCB is access procedure (session: Integer; offset: Unsigned_64);
	
	type drq_access is access dataRequestTask;
	
	type Atomic_Boolean is new Boolean with Atomic;
	package BAO is new System.Atomic_Operations.Exchange(Atomic_Boolean);
	
	dataRequestPending	: aliased Atomic_Boolean;
	
	-- 
	function init(capacity: Unsigned_32) return Boolean;
	function cleanup return Boolean;
	procedure setSeekRequestCallback(cb: seekRequestCB);
	procedure setDataRequestTask(dt: drq_access);
	procedure setSessionHandle(handle: Unsigned_32);
	function getSessionHandle return Unsigned_32;
	procedure setFileSize(size: Integer_64);
	function getFileSize return Integer_64;
	function start return Boolean;
	procedure requestData;
	function reset return Boolean;
	function seek(mode: DataBufferSeek; offset: Integer_64) return Integer_64;
	function seeking return Boolean;
	function read(len: Unsigned_32; bytes: in out buff_array) return Unsigned_32;
	function write(data: buff_array) return Unsigned_32;
	procedure setEof(eof: Boolean);
	function isEof return Boolean;
	function isEmpty return Boolean;
private
	-- private
	type BufferState is
		(DBS_IDLE,
		DBS_BUFFERING,
		DBS_SEEKING);
	
	type buff_ref is access buff_array;
	buffer : buff_ref;
	
	type Atomic_Integer is new Integer with Atomic;
	package IAO is new System.Atomic_Operations.Integer_Arithmetic(Atomic_Integer);
	
	-- Indices into buffer.
	buff_first	: Unsigned_32;	-- Buffer start (0).
	buff_last	: Unsigned_32;	-- Buffer end (capacity - 1).
	data_front	: Unsigned_32;	-- Front of data in buffer (low).
	data_back	: Unsigned_32;	-- Back of data in buffer (last byte + 1).
	read_index	: Unsigned_32;	-- First unread byte or buffer start.
	
	capacity 	: Unsigned_32;
	size		: Unsigned_32;
	filesize	: Integer_64;
	--unread		: Unsigned_32 with Atomic;
	unread		: aliased Atomic_Integer;
	--free		: Unsigned_32 with Atomic;
	free		: aliased Atomic_Integer;
	byteIndex	: Unsigned_32;
	byteIndexLow	: Unsigned_32;
	byteIndexHigh	: Unsigned_32;
	
	eof		: Boolean with Atomic;
	state	: BufferState with Atomic;
	
	seekCB	: seekRequestCB;
	readT	: drq_access;
	
	--seekRequestPending	: Boolean with Atomic;
	seekRequestPending	: aliased Atomic_Boolean;
	--resetRequest		: Boolean with Atomic;
	resetRequest		: aliased Atomic_Boolean;
	
	sessionHandle	: Unsigned_32;
	
	
end LFRingDataBuffer;
