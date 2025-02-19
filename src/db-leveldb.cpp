#include <stdexcept>
#include <sstream>
#include <algorithm>
#include "db-leveldb.h"
#include "types.h"

static inline int64_t stoi64(const std::string &s)
{
	std::istringstream tmp(s);
	int64_t t;
	tmp >> t;
	return t;
}

static inline std::string i64tos(int64_t i)
{
	std::ostringstream os;
	os << i;
	return os.str();
}

// finds the first position in the list where it.x >= x
#define lower_bound_x(container, find_x) \
	std::lower_bound((container).begin(), (container).end(), (find_x), \
		[] (const vec2 &left, int16_t right) { \
			return left.x < right; \
	})

DBLevelDB::DBLevelDB(const std::string &mapdir)
{
	leveldb::Options options;
	options.create_if_missing = false;
	leveldb::Status status = leveldb::DB::Open(options, mapdir + "map.db", &db);
	if (!status.ok()) {
		throw std::runtime_error(std::string("Failed to open database: ") + status.ToString());
	}

	/* LevelDB is a dumb key-value store, so the only optimization we can do
	 * is to cache the block positions that exist in the db.
	 */
	loadPosCache();
}


DBLevelDB::~DBLevelDB()
{
	delete db;
}


std::vector<BlockPos> DBLevelDB::getBlockPosXZ(BlockPos min, BlockPos max)
{
	std::vector<BlockPos> res;
	for (const auto &it : posCache) {
		const int16_t zpos = it.first;
		if (zpos < min.z || zpos >= max.z)
			continue;
		auto it2 = lower_bound_x(it.second, min.x);
		for (; it2 != it.second.end(); it2++) {
			const auto &pos2 = *it2;
			if (pos2.x >= max.x)
				break; // went past
			if (pos2.y < min.y || pos2.y >= max.y)
				continue;
			// skip duplicates
			if (!res.empty() && res.back().x == pos2.x && res.back().z == zpos)
				continue;
			res.emplace_back(pos2.x, pos2.y, zpos);
		}
	}
	return res;
}


void DBLevelDB::loadPosCache()
{
	leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		int64_t posHash = stoi64(it->key().ToString());
		BlockPos pos = decodeBlockPos(posHash);

		posCache[pos.z].emplace_back(pos.x, pos.y);
	}
	delete it;

	for (auto &it : posCache)
		std::sort(it.second.begin(), it.second.end());
}


void DBLevelDB::getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y)
{
	std::string datastr;
	leveldb::Status status;

	auto it = posCache.find(z);
	if (it == posCache.cend())
		return;
	auto it2 = lower_bound_x(it->second, x);
	if (it2 == it->second.end() || it2->x != x)
		return;
	// it2 is now pointing to a contigous part where it2->x == x
	for (; it2 != it->second.end(); it2++) {
		const auto &pos2 = *it2;
		if (pos2.x != x)
			break; // went past
		if (pos2.y < min_y || pos2.y >= max_y)
			continue;

		BlockPos pos(x, pos2.y, z);
		status = db->Get(leveldb::ReadOptions(), i64tos(encodeBlockPos(pos)), &datastr);
		if (status.ok()) {
			blocks.emplace_back(
				pos, ustring((unsigned char *) datastr.data(), datastr.size())
			);
		}
	}
}

void DBLevelDB::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	std::string datastr;
	leveldb::Status status;

	for (auto pos : positions) {
		status = db->Get(leveldb::ReadOptions(), i64tos(encodeBlockPos(pos)), &datastr);
		if (status.ok()) {
			blocks.emplace_back(
				pos, ustring((unsigned char *) datastr.data(), datastr.size())
			);
		}
	}
}
