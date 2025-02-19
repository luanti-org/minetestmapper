#pragma once

#include "db.h"
#include <unordered_map>
#include <sqlite3.h>

class SQLite3Base {
public:
	~SQLite3Base();

protected:
	void openDatabase(const char *path, bool readonly = true);

	// check function result or throw error
	inline void check_result(int result, int good = SQLITE_OK)
	{
		if (result != good)
			throw std::runtime_error(sqlite3_errmsg(db));
	}

	// prepare a statement
	inline int prepare(sqlite3_stmt *&stmt, const char *sql)
	{
		return sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	}

	// read string from statement
	static inline std::string read_str(sqlite3_stmt *stmt, int iCol)
	{
		auto *data = reinterpret_cast<const char*>(
			sqlite3_column_text(stmt, iCol));
		return std::string(data);
	}

	// read blob from statement
	static inline ustring read_blob(sqlite3_stmt *stmt, int iCol)
	{
		auto *data = reinterpret_cast<const unsigned char *>(
			sqlite3_column_blob(stmt, iCol));
		size_t size = sqlite3_column_bytes(stmt, iCol);
		return ustring(data, size);
	}

	sqlite3 *db = NULL;
};

class DBSQLite3 : public DB, SQLite3Base {
public:
	DBSQLite3(const std::string &mapdir);
	std::vector<BlockPos> getBlockPosXZ(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
			int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBSQLite3() override;

	bool preferRangeQueries() const override { return newFormat; }

private:
	static inline void getPosRange(int64_t &min, int64_t &max, int16_t zPos,
			int16_t zPos2);
	void loadBlockCache(int16_t zPos);

	// bind pos to statement. returns index of next column.
	inline int bind_pos(sqlite3_stmt *stmt, int iCol, BlockPos pos)
	{
		if (newFormat) {
			sqlite3_bind_int(stmt, iCol, pos.x);
			sqlite3_bind_int(stmt, iCol + 1, pos.y);
			sqlite3_bind_int(stmt, iCol + 2, pos.z);
			return iCol + 3;
		} else {
			sqlite3_bind_int64(stmt, iCol, encodeBlockPos(pos));
			return iCol + 1;
		}
	}

	sqlite3_stmt *stmt_get_block_pos = NULL;
	sqlite3_stmt *stmt_get_block_pos_range = NULL;
	sqlite3_stmt *stmt_get_blocks_z = NULL;
	sqlite3_stmt *stmt_get_blocks_xz_range = NULL;
	sqlite3_stmt *stmt_get_block_exact = NULL;

	bool newFormat = false;
	int16_t blockCachedZ = -10000;
	std::unordered_map<int16_t, BlockList> blockCache; // indexed by X
};
