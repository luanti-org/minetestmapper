#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <arpa/inet.h>
#include "db-postgresql.h"
#include "util.h"
#include "types.h"

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

/* PostgreSQLBase */

PostgreSQLBase::~PostgreSQLBase()
{
	if (db)
		PQfinish(db);
}

void PostgreSQLBase::openDatabase(const char *connect_string)
{
	if (db)
		throw std::logic_error("Database already open");

	db = PQconnectdb(connect_string);
	if (PQstatus(db) != CONNECTION_OK) {
		throw std::runtime_error(std::string("PostgreSQL database error: ") +
			PQerrorMessage(db)
		);
	}
}

PGresult *PostgreSQLBase::checkResults(PGresult *res, bool clear)
{
	ExecStatusType statusType = PQresultStatus(res);

	switch (statusType) {
	case PGRES_COMMAND_OK:
	case PGRES_TUPLES_OK:
		break;
	case PGRES_FATAL_ERROR:
		throw std::runtime_error(
			std::string("PostgreSQL database error: ") +
			PQresultErrorMessage(res)
		);
	default:
		throw std::runtime_error(
			std::string("Unhandled PostgreSQL result code ") +
			std::to_string(statusType)
		);
	}

	if (clear)
		PQclear(res);
	return res;
}

PGresult *PostgreSQLBase::execPrepared(
	const char *stmtName, const int paramsNumber,
	const void **params,
	const int *paramsLengths, const int *paramsFormats,
	bool clear)
{
	return checkResults(PQexecPrepared(db, stmtName, paramsNumber,
		(const char* const*) params, paramsLengths, paramsFormats,
		1 /* binary output */), clear
	);
}

/* DBPostgreSQL */

DBPostgreSQL::DBPostgreSQL(const std::string &mapdir)
{
	std::ifstream ifs(mapdir + "world.mt");
	if (!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string connect_string = read_setting("pgsql_connection", ifs);
	ifs.close();

	openDatabase(connect_string.c_str());

	prepareStatement(
		"get_block_pos",
		"SELECT posX::int4, posZ::int4 FROM blocks WHERE"
		" (posX BETWEEN $1::int4 AND $2::int4) AND"
		" (posY BETWEEN $3::int4 AND $4::int4) AND"
		" (posZ BETWEEN $5::int4 AND $6::int4) GROUP BY posX, posZ"
	);
	prepareStatement(
		"get_blocks",
		"SELECT posY::int4, data FROM blocks WHERE"
		" posX = $1::int4 AND posZ = $2::int4"
		" AND (posY BETWEEN $3::int4 AND $4::int4)"
	);
	prepareStatement(
		"get_block_exact",
		"SELECT data FROM blocks WHERE"
		" posX = $1::int4 AND posY = $2::int4 AND posZ = $3::int4"
	);

	checkResults(PQexec(db, "START TRANSACTION;"));
	checkResults(PQexec(db, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;"));
}


DBPostgreSQL::~DBPostgreSQL()
{
	try {
		checkResults(PQexec(db, "COMMIT;"));
	} catch (const std::exception& caught) {
		std::cerr << "could not finalize: " << caught.what() << std::endl;
	}
}


std::vector<BlockPos> DBPostgreSQL::getBlockPosXZ(BlockPos min, BlockPos max)
{
	int32_t const x1 = htonl(min.x);
	int32_t const x2 = htonl(max.x - 1);
	int32_t const y1 = htonl(min.y);
	int32_t const y2 = htonl(max.y - 1);
	int32_t const z1 = htonl(min.z);
	int32_t const z2 = htonl(max.z - 1);

	const void *args[] = { &x1, &x2, &y1, &y2, &z1, &z2 };
	const int argLen[] = { 4, 4, 4, 4, 4, 4 };
	const int argFmt[] = { 1, 1, 1, 1, 1, 1 };

	PGresult *results = execPrepared(
		"get_block_pos", ARRLEN(args), args,
		argLen, argFmt, false
	);

	int numrows = PQntuples(results);

	std::vector<BlockPos> positions;
	positions.reserve(numrows);

	BlockPos pos;
	for (int row = 0; row < numrows; ++row) {
		pos.x = pg_binary_to_int(results, row, 0);
		pos.z = pg_binary_to_int(results, row, 1);
		positions.push_back(pos);
	}

	PQclear(results);
	return positions;
}


void DBPostgreSQL::getBlocksOnXZ(BlockList &blocks, int16_t xPos, int16_t zPos,
		int16_t min_y, int16_t max_y)
{
	int32_t const x = htonl(xPos);
	int32_t const z = htonl(zPos);
	int32_t const y1 = htonl(min_y);
	int32_t const y2 = htonl(max_y - 1);

	const void *args[] = { &x, &z, &y1, &y2 };
	const int argLen[] = { 4, 4, 4, 4 };
	const int argFmt[] = { 1, 1, 1, 1 };

	PGresult *results = execPrepared(
		"get_blocks", ARRLEN(args), args,
		argLen, argFmt, false
	);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row) {
		BlockPos position;
		position.x = xPos;
		position.y = pg_binary_to_int(results, row, 0);
		position.z = zPos;
		blocks.emplace_back(
			position,
			ustring(
				reinterpret_cast<unsigned char*>(
					PQgetvalue(results, row, 1)
				),
				PQgetlength(results, row, 1)
			)
		);
	}

	PQclear(results);
}


void DBPostgreSQL::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	int32_t x, y, z;

	const void *args[] = { &x, &y, &z };
	const int argLen[] = { 4, 4, 4 };
	const int argFmt[] = { 1, 1, 1 };

	for (auto pos : positions) {
		x = htonl(pos.x);
		y = htonl(pos.y);
		z = htonl(pos.z);

		PGresult *results = execPrepared(
			"get_block_exact", ARRLEN(args), args,
			argLen, argFmt, false
		);

		if (PQntuples(results) > 0) {
			blocks.emplace_back(
				pos,
				ustring(
					reinterpret_cast<unsigned char*>(
						PQgetvalue(results, 0, 0)
					),
					PQgetlength(results, 0, 0)
				)
			);
		}

		PQclear(results);
	}
}

int DBPostgreSQL::pg_binary_to_int(PGresult *res, int row, int col)
{
	int32_t* raw = reinterpret_cast<int32_t*>(PQgetvalue(res, row, col));
	return ntohl(*raw);
}
