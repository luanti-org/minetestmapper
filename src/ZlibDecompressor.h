#pragma once

#include <exception>
#include "types.h"

class ZlibDecompressor
{
public:
	class DecompressError : std::exception {};

	ZlibDecompressor(const u8 *data, size_t size);
	~ZlibDecompressor();
	void setSeekPos(size_t seekPos);
	size_t seekPos() const { return m_seekPos; }
	// Decompress and return one zlib stream from the buffer
	// Advances seekPos as appropriate.
	void decompress(ustring &dst);

private:
	const u8 *m_data;
	size_t m_seekPos, m_size;
};
