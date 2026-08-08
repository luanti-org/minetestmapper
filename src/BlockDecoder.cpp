#include <string>
#include <utility>

#include "BlockDecoder.h"
#include "ZlibDecompressor.h"
#include "log.h"

namespace {

class BufferReader {
public:
	BufferReader() = default;
	BufferReader(const ustring &s) : buffer(s.c_str()), length(s.length()) {}

	/// @brief Returns a pointer to the requested amount of bytes and advances the offset
	const unsigned char *ptr(size_t requested);
	bool eof() const {
		return offset == length;
	}
	size_t remaining() const {
		return length - offset;
	}
	void skip(size_t requested) {
		(void)ptr(requested);
	}
	uint8_t u8() {
		return *ptr(1);
	}
	uint16_t u16() {
		auto *p = ptr(2);
		return (p[0] << 8) | p[1];
	}
	std::string str(size_t requested) {
		if (requested == 0)
			return {};
		return std::string(reinterpret_cast<const char*>(ptr(requested)), requested);
	}

private:
	const unsigned char *buffer = nullptr;
	size_t offset = 0, length = 0;
};

const unsigned char *BufferReader::ptr(size_t requested)
{
	if (length - offset < requested) {
		std::string msg("Reading outside buffer: offset=");
		msg.append(std::to_string(offset)).append(" length=").append(std::to_string(length))
			.append(" requested=").append(std::to_string(requested));
		throw std::runtime_error(msg);
	}
	auto *ret = &buffer[offset];
	offset += requested;
	return ret;
}

}

static inline uint16_t readBlockContent(const unsigned char *mapData,
	u8 contentWidth, unsigned int datapos)
{
	if (contentWidth == 2) {
		size_t index = datapos << 1;
		return (mapData[index] << 8) | mapData[index + 1];
	} else {
		u8 param = mapData[datapos];
		if (param <= 0x7f)
			return param;
		else
			return (param << 4) | (mapData[datapos + 0x2000] >> 4);
	}
}

BlockDecoder::BlockDecoder()
{
	reset();
}

void BlockDecoder::reset()
{
	m_blockAirId = -1;
	m_blockIgnoreId = -1;
	m_nameMap.clear();

	m_version = 0;
	m_contentWidth = 0;
	m_mapData.clear();
}

void BlockDecoder::decode(const ustring &datastr)
{
	BufferReader reader(datastr);

	uint8_t version = reader.u8();
	if (version < 22) {
		auto err = "Unsupported map version " + std::to_string(version);
		throw std::runtime_error(err);
	}
	m_version = version;

	if (version >= 29) {
		// decompress whole block at once
		m_zstd_decompressor.setData(reader.ptr(0), reader.remaining(), 0);
		m_zstd_decompressor.decompress(m_scratch);
		reader = BufferReader(m_scratch);
	}

	if (version >= 29)
		reader.skip(7);
	else if (version >= 27)
		reader.skip(3);
	else
		reader.skip(1);

	auto decode_mapping = [&] () {
		reader.skip(1); // mapping version
		uint16_t numMappings = reader.u16();
		for (int i = 0; i < numMappings; ++i) {
			uint16_t nodeId = reader.u16();
			uint16_t nameLen = reader.u16();
			std::string name = reader.str(nameLen);
			if (name == "air")
				m_blockAirId = nodeId;
			else if (name == "ignore")
				m_blockIgnoreId = nodeId;
			else
				m_nameMap[nodeId] = std::move(name);
		}
	};

	// Mapping comes early
	if (version >= 29)
		decode_mapping();

	uint8_t contentWidth = reader.u8();
	uint8_t paramsWidth = reader.u8();
	if (contentWidth != 1 && contentWidth != 2) {
		auto err = "Unsupported map version contentWidth=" + std::to_string(contentWidth);
		throw std::runtime_error(err);
	}
	if (paramsWidth != 2) {
		auto err = "Unsupported map version paramsWidth=" + std::to_string(paramsWidth);
		throw std::runtime_error(err);
	}
	m_contentWidth = contentWidth;
	const size_t mapDataSize = (contentWidth + paramsWidth) * 4096;

	if (version >= 29) {
		m_mapData.assign(reader.ptr(mapDataSize), mapDataSize);
		return; // we have read everything we need and can return early
	}

	// version < 29
	ZlibDecompressor decompressor(reader.ptr(0), reader.remaining());
	decompressor.decompress(m_mapData);
	decompressor.decompress(m_scratch); // unused metadata
	reader.skip(decompressor.seekPos());

	if (m_mapData.size() < mapDataSize)
		throw std::runtime_error("Map data buffer truncated");

	// Skip unused node timers
	if (version == 23)
		reader.skip(1);
	if (version == 24) {
		uint8_t ver = reader.u8();
		if (ver == 1) {
			uint16_t num = reader.u16();
			reader.skip(10 * num);
		}
	}

	// Skip unused static objects
	reader.skip(1); // static object version
	uint16_t staticObjectCount = reader.u16();
	for (int i = 0; i < staticObjectCount; ++i) {
		reader.skip(13);
		uint16_t dataSize = reader.u16();
		reader.skip(dataSize);
	}
	reader.skip(4); // timestamp

	// Read mapping
	decode_mapping();
}

bool BlockDecoder::isEmpty() const
{
	// only contains ignore and air nodes?
	return m_nameMap.empty();
}

const static std::string empty;

const std::string &BlockDecoder::getNode(u8 x, u8 y, u8 z) const
{
	unsigned int position = x + (y << 4) + (z << 8);
	uint16_t content = readBlockContent(m_mapData.c_str(), m_contentWidth, position);
	if (content == m_blockAirId || content == m_blockIgnoreId)
		return empty;
	auto it = m_nameMap.find(content);
	if (it == m_nameMap.end()) {
		errorstream << "Skipping node with invalid ID " << (int)content << std::endl;
		return empty;
	}
	return it->second;
}
