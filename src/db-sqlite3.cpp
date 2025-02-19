#include <stdexcept>
#include <unistd.h> // for usleep
#include <iostream>
#include <algorithm>
#include <cassert>
#include "db-sqlite3.h"
#include "types.h"

/* SQLite3Base */

#define SQLRES(r, good) check_result(r, good)
#define SQLOK(r) SQLRES(r, SQLITE_OK)

SQLite3Base::~SQLite3Base()
{
	if (db && sqlite3_close(db) != SQLITE_OK) {
		std::cerr << "Error closing SQLite database: "
			<< sqlite3_errmsg(db) << std::endl;
	}
}

void SQLite3Base::openDatabase(const char *path, bool readonly)
{
	if (db)
		throw std::logic_error("Database already open");

	int flags = 0;
	if (readonly)
		flags |= SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE;
	else
		flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
#ifdef SQLITE_OPEN_EXRESCODE
	flags |= SQLITE_OPEN_EXRESCODE;
#endif
	SQLOK(sqlite3_open_v2(path, &db, flags, 0));
}

/* DBSQLite3 */

// make sure a row is available. intended to be used outside a loop.
// compare result to SQLITE_ROW afterwards.
#define SQLROW1(stmt) \
	while ((result = sqlite3_step(stmt)) == SQLITE_BUSY) \
		usleep(10000); /* wait some time and try again */ \
	if (result != SQLITE_ROW && result != SQLITE_DONE) { \
		throw std::runtime_error(sqlite3_errmsg(db)); \
	}

// make sure next row is available. intended to be used in a while(sqlite3_step) loop
#define SQLROW2() \
	if (result == SQLITE_BUSY) { \
		usleep(10000); /* wait some time and try again */ \
		continue; \
	} else if (result != SQLITE_ROW) { \
		throw std::runtime_error(sqlite3_errmsg(db)); \
	}

DBSQLite3::DBSQLite3(const std::string &mapdir)
{
	std::string db_name = mapdir + "map.sqlite";

	openDatabase(db_name.c_str());

	// There's a simple, dumb way to check if we have a new or old database schema.
	// If we prepare a statement that references columns that don't exist, it will
	// error right there.
	int result = prepare(stmt_get_block_pos, "SELECT x, y, z FROM blocks");
	newFormat = result == SQLITE_OK;
#ifndef NDEBUG
	std::cerr << "Detected " << (newFormat ? "new" : "old") << " SQLite schema" << std::endl;
#endif

	if (newFormat) {
		SQLOK(prepare(stmt_get_blocks_xz_range,
				"SELECT y, data FROM blocks WHERE "
				"x = ? AND z = ? AND y BETWEEN ? AND ?"));

		SQLOK(prepare(stmt_get_block_exact,
				"SELECT data FROM blocks WHERE x = ? AND y = ? AND z = ?"));

		SQLOK(prepare(stmt_get_block_pos_range,
				"SELECT x, z FROM blocks WHERE "
				"x >= ? AND y >= ? AND z >= ? AND "
				"x < ? AND y < ? AND z < ? GROUP BY x, z"));
	} else {
		SQLOK(prepare(stmt_get_blocks_z,
				"SELECT pos, data FROM blocks WHERE pos BETWEEN ? AND ?"));

		SQLOK(prepare(stmt_get_block_exact,
				"SELECT data FROM blocks WHERE pos = ?"));

		SQLOK(prepare(stmt_get_block_pos,
				"SELECT pos FROM blocks"));

		SQLOK(prepare(stmt_get_block_pos_range,
				"SELECT pos FROM blocks WHERE pos BETWEEN ? AND ?"));
	}

#undef RANGE
}


DBSQLite3::~DBSQLite3()
{
	sqlite3_finalize(stmt_get_blocks_z);
	sqlite3_finalize(stmt_get_blocks_xz_range);
	sqlite3_finalize(stmt_get_block_pos);
	sqlite3_finalize(stmt_get_block_pos_range);
	sqlite3_finalize(stmt_get_block_exact);
}


inline void DBSQLite3::getPosRange(int64_t &min, int64_t &max,
		int16_t zPos, int16_t zPos2)
{
	// Magic numbers!
	min = encodeBlockPos(BlockPos(0, -2048, zPos));
	max = encodeBlockPos(BlockPos(0, 2048, zPos2)) - 1;
}


std::vector<BlockPos> DBSQLite3::getBlockPosXZ(BlockPos min, BlockPos max)
{
	int result;
	sqlite3_stmt *stmt;

	if (newFormat) {
		stmt = stmt_get_block_pos_range;
		int col = bind_pos(stmt, 1, min);
		bind_pos(stmt, col, max);
	} else {
		// can handle range query on Z axis via SQL
		if (min.z <= -2048 && max.z >= 2048) {
			stmt = stmt_get_block_pos;
		} else {
			stmt = stmt_get_block_pos_range;
			int64_t minPos, maxPos;
			if (min.z < -2048)
				min.z = -2048;
			if (max.z > 2048)
				max.z = 2048;
			getPosRange(minPos, maxPos, min.z, max.z - 1);
			SQLOK(sqlite3_bind_int64(stmt, 1, minPos));
			SQLOK(sqlite3_bind_int64(stmt, 2, maxPos));
		}
	}

	std::vector<BlockPos> positions;
	BlockPos pos;
	while ((result = sqlite3_step(stmt)) != SQLITE_DONE) {
		SQLROW2()

		if (newFormat) {
			pos.x = sqlite3_column_int(stmt, 0);
			pos.z = sqlite3_column_int(stmt, 1);
		} else {
			pos = decodeBlockPos(sqlite3_column_int64(stmt, 0));
			if (pos.x < min.x || pos.x >= max.x || pos.y < min.y || pos.y >= max.y)
				continue;
			// note that we can't try to deduplicate these because the order
			// of the encoded pos (if sorted) is ZYX.
		}
		positions.emplace_back(pos);
	}
	SQLOK(sqlite3_reset(stmt));
	return positions;
}


void DBSQLite3::loadBlockCache(int16_t zPos)
{
	int result;
	blockCache.clear();

	assert(!newFormat);

	int64_t minPos, maxPos;
	getPosRange(minPos, maxPos, zPos, zPos);

	SQLOK(sqlite3_bind_int64(stmt_get_blocks_z, 1, minPos));
	SQLOK(sqlite3_bind_int64(stmt_get_blocks_z, 2, maxPos));

	while ((result = sqlite3_step(stmt_get_blocks_z)) != SQLITE_DONE) {
		SQLROW2()

		int64_t posHash = sqlite3_column_int64(stmt_get_blocks_z, 0);
		BlockPos pos = decodeBlockPos(posHash);
		blockCache[pos.x].emplace_back(pos, read_blob(stmt_get_blocks_z, 1));
	}
	SQLOK(sqlite3_reset(stmt_get_blocks_z));
}


void DBSQLite3::getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y)
{
	// New format: use a real range query
	if (newFormat) {
		auto *stmt = stmt_get_blocks_xz_range;
		SQLOK(sqlite3_bind_int(stmt, 1, x));
		SQLOK(sqlite3_bind_int(stmt, 2, z));
		SQLOK(sqlite3_bind_int(stmt, 3, min_y));
		SQLOK(sqlite3_bind_int(stmt, 4, max_y - 1)); // BETWEEN is inclusive

		int result;
		while ((result = sqlite3_step(stmt)) != SQLITE_DONE) {
			SQLROW2()

			BlockPos pos(x, sqlite3_column_int(stmt, 0), z);
			blocks.emplace_back(pos, read_blob(stmt, 1));
		}
		SQLOK(sqlite3_reset(stmt));
		return;
	}

	/* Cache the blocks on the given Z coordinate between calls, this only
	 * works due to order in which the TileGenerator asks for blocks. */
	if (z != blockCachedZ) {
		loadBlockCache(z);
		blockCachedZ = z;
	}

	auto it = blockCache.find(x);
	if (it == blockCache.end())
		return;

	if (it->second.empty()) {
		/* We have swapped this list before, this is not supposed to happen
		 * because it's bad for performance. But rather than silently breaking
		 * do the right thing and load the blocks again. */
#ifndef NDEBUG
		std::cerr << "Warning: suboptimal access pattern for sqlite3 backend" << std::endl;
#endif
		loadBlockCache(z);
	}
	// Swap lists to avoid copying contents
	blocks.clear();
	std::swap(blocks, it->second);

	for (auto it = blocks.begin(); it != blocks.end(); ) {
		if (it->first.y < min_y || it->first.y >= max_y)
			it = blocks.erase(it);
		else
			it++;
	}
}


void DBSQLite3::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	int result;

	for (auto pos : positions) {
		bind_pos(stmt_get_block_exact, 1, pos);

		SQLROW1(stmt_get_block_exact)
		if (result == SQLITE_ROW)
			blocks.emplace_back(pos, read_blob(stmt_get_block_exact, 0));

		SQLOK(sqlite3_reset(stmt_get_block_exact));
	}
}
