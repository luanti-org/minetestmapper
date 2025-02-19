#include <stdexcept>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include "util.h"

static std::string trim(const std::string &s)
{
	auto isspace = [] (char c) {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	};

	size_t front = 0;
	while (isspace(s[front]))
		++front;
	size_t back = s.size() - 1;
	while (back > front && isspace(s[back]))
		--back;

	return s.substr(front, back - front + 1);
}

std::string read_setting(const std::string &name, std::istream &is)
{
	char linebuf[512];
	is.seekg(0);
	while (is.good()) {
		is.getline(linebuf, sizeof(linebuf));

		for (char *p = linebuf; *p; p++) {
			if(*p != '#')
				continue;
			*p = '\0'; // Cut off at the first #
			break;
		}
		std::string line(linebuf);

		auto pos = line.find('=');
		if (pos == std::string::npos)
			continue;
		auto key = trim(line.substr(0, pos));
		if (key != name)
			continue;
		return trim(line.substr(pos+1));
	}
	std::ostringstream oss;
	oss << "Setting '" << name << "' not found";
	throw std::runtime_error(oss.str());
}

std::string read_setting_default(const std::string &name, std::istream &is,
	const std::string &def)
{
	try {
		return read_setting(name, is);
	} catch(const std::runtime_error &e) {
		return def;
	}
}

bool file_exists(const char *path)
{
	struct stat s{};
	// check for !dir to allow symlinks or such
	return stat(path, &s) == 0 && (s.st_mode & S_IFDIR) != S_IFDIR;
}

bool dir_exists(const char *path)
{
	struct stat s{};
	return stat(path, &s) == 0 && (s.st_mode & S_IFDIR) == S_IFDIR;
}
