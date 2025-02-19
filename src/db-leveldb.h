#pragma once

#include "db.h"
#include <unordered_map>
#include <utility>
#include <leveldb/db.h>

class DBLevelDB : public DB {
public:
	DBLevelDB(const std::string &mapdir);
	std::vector<BlockPos> getBlockPosXZ(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
			int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBLevelDB() override;

	bool preferRangeQueries() const override { return false; }

private:
	struct vec2 {
		int16_t x, y;
		constexpr vec2() : x(0), y(0) {}
		constexpr vec2(int16_t x, int16_t y) : x(x), y(y) {}

		inline bool operator<(const vec2 &p) const
		{
			if (x < p.x)
				return true;
			if (x > p.x)
				return false;
			return y < p.y;
		}
	};

	void loadPosCache();

	// indexed by Z, contains all (x,y) position pairs
	std::unordered_map<int16_t, std::vector<vec2>> posCache;
	leveldb::DB *db = NULL;
};
