#pragma once

#include <string>
#include <iostream>

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

template<typename T>
static inline T mymax(T a, T b)
{
	return (a > b) ? a : b;
}

template<typename T>
static inline T mymin(T a, T b)
{
	return (a > b) ? b : a;
}

std::string read_setting(const std::string &name, std::istream &is);

std::string read_setting_default(const std::string &name, std::istream &is,
	const std::string &def);

bool file_exists(const char *path);

bool dir_exists(const char *path);
