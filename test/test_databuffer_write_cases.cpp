/*
 * test_databuffer_write_cases.cpp
 */

#include "../src/databuffer.h"

#include <cassert>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>

// DataBuffer::write() uses `char *`, not `uint8_t` or `unsigned char *`.

char * to_char_ptr(uint8_t * data)
{
	return reinterpret_cast<char *>(data);
}

// Fill data monotonically increasing, starting at `start`, return next value to use.

int fill(std::vector<uint8_t> & data, int start)
{
	std::iota(data.begin(), data.end(), start);
	return start + data.size();
}

// Return True if data is increasing monotonically.

bool is_monotonic(std::vector<uint8_t> const & data)
{
	// prevent out-of-range indexing.
	if (data.size() <= 1)
		return true;

	for (int i = 0; i < data.size() - 1; ++i)
	{
		if (data[i + 1] - data[i] != 1)
			return false;
	}
	return true;
}

// Return "monotonic", or "non-monotonic", depending on data monotonicity.

auto str_monotonic(std::vector<uint8_t> const & data) -> std::string
{
	return is_monotonic(data) ? "monotonic" : "non-monotonic";
}

// Report the content read, return True if it is monotonically increasing.

bool report(std::string title, std::vector<uint8_t> const & data)
{
	std::cout << title << ": [";
	std::copy(data.begin(), data.end(), std::ostream_iterator<int>(std::cout, ", "));
	std::cout << "]: " << str_monotonic(data) << "\n";
	return is_monotonic(data);
}

// Separate read to use in non-zero-sized reads (prevent creating zero-sized read buffers).
// Report the content read, return True if it is monotonically increasing.

bool read(int Nread)
{
	std::vector<uint8_t> Bread(Nread);

	assert(Nread = DataBuffer::read(Nread, Bread.data()));
	return report("Bread", Bread);
}

// Test write, read sequence specified bij buffer capacity, prefill (warmup), partial read and write.

void test_databuffer_case(std::string const & title, int capacity, int Nwarmup, int Nread, int Nwrite, std::string const & note)
{
	std::cout <<
		"\n" << title <<
		" capacity:"  << capacity <<
		" warmup:"    << Nwarmup  <<
		" read:"      << Nread    <<
		" write:"     << Nwrite   <<
		" - "         << note     <<
		"\n";

	//  Create ring buffer.
	
	if (not DataBuffer::init(capacity)) {
		std::cout << "DB Init failed.\n";
		return;
    }

	// Fill buffers with monotonically increasing data to write to the ring buffer.

	std::vector<uint8_t> Bwarmup(Nwarmup);
	std::vector<uint8_t> Bwrite(Nwrite);

	int value = 0;
	value = fill(Bwarmup, value);
	value = fill(Bwrite, value);

	assert(report("Bwarmup", Bwarmup));			// Assert monotonicity
	assert(report("Bwrite", Bwrite));			// Assert monotonicity

	assert(Nwarmup = DataBuffer::write(to_char_ptr(Bwarmup.data()), Bwarmup.size()));
	if (Nread > 0)
		assert(read(Nread));					// Assert monotonicity
	const int Nactual = DataBuffer::write(to_char_ptr(Bwrite.data()), Bwrite.size());
	assert(Nactual <= Nwrite);					// Not all may fit in ring buffer.
	assert(read(Nwarmup - Nread + Nactual));	// Read all what's in ring buffer, asserting monotonicity.
}

// Main program.

int main()
{
	// 								Ring buffer size
	//                              |  Nwarmup
	//                              |  |  Nread
	//                              |  |  |  Nwrite
	test_databuffer_case("Test 1:", 7, 3, 0, 3, "Single write at back");				// branch: if data'Length <= bytesSingleWrite then
	test_databuffer_case("Test 2:", 7, 4, 0, 4, "Single write at back, only part");		// branch: elsif bytesSingleWrite > 0 and locfree = bytesSingleWrite then
	test_databuffer_case("Test 3:", 7, 5, 3, 5, "Partly write at back, rest at front");	// branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
	test_databuffer_case("Test 4:", 7, 5, 3, 6, "Partly write at back, part at front");	// branch: elsif bytesSingleWrite > 0 and locfree > bytesSingleWrite then
}
