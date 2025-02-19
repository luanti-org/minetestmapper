#include <stdexcept>
#include <iostream>
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

static bool read_setting(const std::string &name, std::istream &is, std::string &out)
{
	char linebuf[512];
	is.seekg(0);
	while (is.good()) {
		is.getline(linebuf, sizeof(linebuf));
		std::string line(linebuf);

		auto pos = line.find('#');
		if (pos != std::string::npos)
			line.erase(pos); // remove comments

		pos = line.find('=');
		if (pos == std::string::npos)
			continue;
		auto key = trim(line.substr(0, pos));
		if (key != name)
			continue;
		out = trim(line.substr(pos+1));
		return true;
	}
	return false;
}

std::string read_setting(const std::string &name, std::istream &is)
{
	std::string ret;
	if (!read_setting(name, is, ret))
		throw std::runtime_error(std::string("Setting not found: ") + name);
	return ret;
}

std::string read_setting_default(const std::string &name, std::istream &is,
	const std::string &def)
{
	std::string ret;
	if (!read_setting(name, is, ret))
		return def;
	return ret;
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
