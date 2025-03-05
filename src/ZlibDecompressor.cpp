#include <cstdint>
#include "ZlibDecompressor.h"
#include "config.h"

// for convenient usage of both
#if USE_ZLIB_NG
#include <zlib-ng.h>
#define z_stream zng_stream
#define Z(x) zng_ ## x
#else
#include <zlib.h>
#define Z(x) x
#endif

ZlibDecompressor::ZlibDecompressor(const u8 *data, size_t size):
	m_data(data),
	m_seekPos(0),
	m_size(size)
{
}

ZlibDecompressor::~ZlibDecompressor()
{
}

void ZlibDecompressor::setSeekPos(size_t seekPos)
{
	m_seekPos = seekPos;
}

size_t ZlibDecompressor::seekPos() const
{
	return m_seekPos;
}

ustring ZlibDecompressor::decompress()
{
	const unsigned char *data = m_data + m_seekPos;
	const size_t size = m_size - m_seekPos;

	ustring buffer;
	constexpr size_t BUFSIZE = 32 * 1024;

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = Z_NULL;
	strm.avail_in = 0;

	if (Z(inflateInit)(&strm) != Z_OK)
		throw DecompressError();

	strm.next_in = const_cast<unsigned char *>(data);
	strm.avail_in = size;
	buffer.resize(BUFSIZE);
	strm.next_out = &buffer[0];
	strm.avail_out = BUFSIZE;

	int ret = 0;
	do {
		ret = Z(inflate)(&strm, Z_NO_FLUSH);
		if (strm.avail_out == 0) {
			const auto off = buffer.size();
			buffer.reserve(off + BUFSIZE);
			strm.next_out = &buffer[off];
			strm.avail_out = BUFSIZE;
		}
	} while (ret == Z_OK);
	if (ret != Z_STREAM_END)
		throw DecompressError();

	m_seekPos += strm.next_in - data;
	buffer.resize(buffer.size() - strm.avail_out);
	(void) Z(inflateEnd)(&strm);

	return buffer;
}

