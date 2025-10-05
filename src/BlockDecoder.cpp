#include <string>
#include <utility>

#include "BlockDecoder.h"
#include "ZlibDecompressor.h"
#include "log.h"

static inline uint16_t readU16(const unsigned char *data)
{
	return data[0] << 8 | data[1];
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
	const unsigned char *data = datastr.c_str();
	size_t length = datastr.length();
	// TODO: Add strict bounds checks everywhere

	uint8_t version = data[0];
	if (version < 22) {
		auto err = "Unsupported map version " + std::to_string(version);
		throw std::runtime_error(err);
	}
	m_version = version;

	if (version >= 29) {
		// decompress whole block at once
		m_zstd_decompressor.setData(data, length, 1);
		m_zstd_decompressor.decompress(m_scratch);
		data = m_scratch.c_str();
		length = m_scratch.size();
	}

	size_t dataOffset = 0;
	if (version >= 29)
		dataOffset = 7;
	else if (version >= 27)
		dataOffset = 4;
	else
		dataOffset = 2;

	auto decode_mapping = [&] () {
		dataOffset++; // mapping version
		uint16_t numMappings = readU16(data + dataOffset);
		dataOffset += 2;
		for (int i = 0; i < numMappings; ++i) {
			uint16_t nodeId = readU16(data + dataOffset);
			dataOffset += 2;
			uint16_t nameLen = readU16(data + dataOffset);
			dataOffset += 2;
			std::string name(reinterpret_cast<const char *>(data) + dataOffset, nameLen);
			if (name == "air")
				m_blockAirId = nodeId;
			else if (name == "ignore")
				m_blockIgnoreId = nodeId;
			else
				m_nameMap[nodeId] = std::move(name);
			dataOffset += nameLen;
		}
	};

	if (version >= 29)
		decode_mapping();

	uint8_t contentWidth = data[dataOffset];
	dataOffset++;
	uint8_t paramsWidth = data[dataOffset];
	dataOffset++;
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
		if (length < dataOffset + mapDataSize)
			throw std::runtime_error("Map data buffer truncated");
		m_mapData.assign(data + dataOffset, mapDataSize);
		return; // we have read everything we need and can return early
	}

	// version < 29
	ZlibDecompressor decompressor(data, length);
	decompressor.setSeekPos(dataOffset);
	decompressor.decompress(m_mapData);
	decompressor.decompress(m_scratch); // unused metadata
	dataOffset = decompressor.seekPos();

	if (m_mapData.size() < mapDataSize)
		throw std::runtime_error("Map data buffer truncated");

	// Skip unused node timers
	if (version == 23)
		dataOffset += 1;
	if (version == 24) {
		uint8_t ver = data[dataOffset++];
		if (ver == 1) {
			uint16_t num = readU16(data + dataOffset);
			dataOffset += 2;
			dataOffset += 10 * num;
		}
	}

	// Skip unused static objects
	dataOffset++; // Skip static object version
	uint16_t staticObjectCount = readU16(data + dataOffset);
	dataOffset += 2;
	for (int i = 0; i < staticObjectCount; ++i) {
		dataOffset += 13;
		uint16_t dataSize = readU16(data + dataOffset);
		dataOffset += dataSize + 2;
	}
	dataOffset += 4; // Skip timestamp

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
	NameMap::const_iterator it = m_nameMap.find(content);
	if (it == m_nameMap.end()) {
		errorstream << "Skipping node with invalid ID " << (int)content << std::endl;
		return empty;
	}
	return it->second;
}
