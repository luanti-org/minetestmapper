#include <fstream>
#include <sstream>
#include <stdexcept>
#include <dirent.h>
#include <unistd.h> // usleep

#include "config.h"
#include "PlayerAttributes.h"
#include "util.h"
#include "log.h"
#include "db-sqlite3.h" // SQLite3Base

namespace {
	bool parse_pos(std::string position, Player &dst)
	{
		if (position.empty())
			return false;
		if (position.front() == '(' && position.back() == ')')
			position = position.substr(1, position.size() - 2);
		std::istringstream iss(position);
		if (!(iss >> dst.x))
			return false;
		if (iss.get() != ',')
			return false;
		if (!(iss >> dst.y))
			return false;
		if (iss.get() != ',')
			return false;
		if (!(iss >> dst.z))
			return false;
		return iss.eof();
	}

	// Helper classes per backend

	class FilesReader {
		std::string path;
		DIR *dir = nullptr;
	public:
		FilesReader(const std::string &path) : path(path) {
			dir = opendir(path.c_str());
		}
		~FilesReader() {
			if (dir)
				closedir(dir);
		}

		void read(PlayerAttributes::Players &dest);
	};

	class SQLiteReader : SQLite3Base {
		sqlite3_stmt *stmt_get_player_pos = NULL;
	public:
		SQLiteReader(const std::string &database) {
			openDatabase(database.c_str());
		}
		~SQLiteReader() {
			sqlite3_finalize(stmt_get_player_pos);
		}

		void read(PlayerAttributes::Players &dest);
	};
}

void FilesReader::read(PlayerAttributes::Players &dest)
{
	if (!dir)
		return;

	struct dirent *ent;
	std::string name, position;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		std::ifstream in(path + PATH_SEPARATOR + ent->d_name);
		if (!in.good())
			continue;

		name = read_setting("name", in);
		position = read_setting("position", in);

		Player player;
		player.name = name;
		if (!parse_pos(position, player)) {
			errorstream << "Failed to parse position '" << position << "' in "
				<< ent->d_name << std::endl;
			continue;
		}

		player.x /= 10.0f;
		player.y /= 10.0f;
		player.z /= 10.0f;

		dest.push_back(std::move(player));
	}
}

#define SQLRES(r, good) check_result(r, good)
#define SQLOK(r) SQLRES(r, SQLITE_OK)

void SQLiteReader::read(PlayerAttributes::Players &dest)
{
	SQLOK(prepare(stmt_get_player_pos,
			"SELECT name, posX, posY, posZ FROM player"));

	int result;
	while ((result = sqlite3_step(stmt_get_player_pos)) != SQLITE_DONE) {
		if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		}

		Player player;
		player.name = read_str(stmt_get_player_pos, 0);
		player.x = sqlite3_column_double(stmt_get_player_pos, 1);
		player.y = sqlite3_column_double(stmt_get_player_pos, 2);
		player.z = sqlite3_column_double(stmt_get_player_pos, 3);

		player.x /= 10.0f;
		player.y /= 10.0f;
		player.z /= 10.0f;

		dest.push_back(std::move(player));
	}
}

/**********/

PlayerAttributes::PlayerAttributes(const std::string &worldDir)
{
	std::ifstream ifs(worldDir + "world.mt");
	if (!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string backend = read_setting_default("player_backend", ifs, "files");
	ifs.close();

	verbosestream << "Player backend is " << backend << std::endl;
	if (backend == "files")
		FilesReader(worldDir + "players").read(m_players);
	else if (backend == "sqlite3")
		SQLiteReader(worldDir + "players.sqlite").read(m_players);
	else
		throw std::runtime_error(std::string("Unknown player backend: ") + backend);
}

PlayerAttributes::Players::const_iterator PlayerAttributes::begin() const
{
	return m_players.cbegin();
}

PlayerAttributes::Players::const_iterator PlayerAttributes::end() const
{
	return m_players.cend();
}

