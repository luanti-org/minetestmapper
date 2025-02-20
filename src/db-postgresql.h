#pragma once

#include "db.h"
#include <libpq-fe.h>

class PostgreSQLBase {
public:
	~PostgreSQLBase();

protected:
	void openDatabase(const char *connect_string);

	PGresult *checkResults(PGresult *res, bool clear = true);
	void prepareStatement(const std::string &name, const std::string &sql) {
		checkResults(PQprepare(db, name.c_str(), sql.c_str(), 0, NULL));
	}
	PGresult *execPrepared(
		const char *stmtName, const int paramsNumber,
		const void **params,
		const int *paramsLengths = nullptr, const int *paramsFormats = nullptr,
		bool clear = true
	);

	PGconn *db = NULL;
};

class DBPostgreSQL : public DB, PostgreSQLBase {
public:
	DBPostgreSQL(const std::string &mapdir);
	std::vector<BlockPos> getBlockPosXZ(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBPostgreSQL() override;

	bool preferRangeQueries() const override { return true; }

private:
	int pg_binary_to_int(PGresult *res, int row, int col);
};
