#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <utility>
#include <string>
#include <sstream>
#include <stdexcept>
#include "config.h"
#include "TileGenerator.h"
#include "util.h"
#include "log.h"

static void usage()
{
	static const std::pair<const char*, const char*> options[] = {
		{"-i/--input", "<path>"},
		{"-o/--output", "<path>"},
		{"--bgcolor", "<color>"},
		{"--scalecolor", "<color>"},
		{"--playercolor", "<color>"},
		{"--origincolor", "<color>"},
		{"--drawscale", ""},
		{"--drawplayers", ""},
		{"--draworigin", ""},
		{"--drawalpha", ""},
		{"--noshading", ""},
		{"--noemptyimage", ""},
		{"-v/--verbose", ""},
		{"--min-y", "<y>"},
		{"--max-y", "<y>"},
		{"--backend", "<backend>"},
		{"--geometry", "x:z+w+h"},
		{"--extent", ""},
		{"--zoom", "<factor>"},
		{"--colors", "<path>"},
		{"--scales", "[t][b][l][r]"},
		{"--exhaustive", "never|y|full|auto"},
		{"--dumpblock", "x,y,z"},
	};
	const char *top_text =
		"minetestmapper -i <world_path> -o <output_image> [options]\n"
		"Generate an overview image of a Luanti map.\n"
		"\n"
		"Options:\n";
	const char *bottom_text =
		"\n"
		"Color format: hexadecimal '#RRGGBB', e.g. '#FF0000' = red\n";

	printf("%s", top_text);
	for (const auto &p : options)
		printf("  %-18s%s\n", p.first, p.second);
	printf("%s", bottom_text);
	auto backends = TileGenerator::getSupportedBackends();
	printf("Supported backends: ");
	for (auto s : backends)
		printf("%s ", s.c_str());
	printf("\n");
#ifdef _WIN32
	printf("See also the full documentation in README.rst\n");
#else
	printf("See also the full documentation in minetestmapper(6) or README.rst\n");
#endif
}

static inline bool file_exists(const std::string &path)
{
	return file_exists(path.c_str());
}

static inline int stoi(const char *s)
{
	std::istringstream iss(s);
	int ret;
	iss >> ret;
	return ret;
}

static std::string search_colors(const std::string &worldpath)
{
	if (file_exists(worldpath + "/colors.txt"))
		return worldpath + "/colors.txt";

#ifndef _WIN32
	char *home = std::getenv("HOME");
	if (home && home[0]) {
		std::string check = std::string(home) + "/.minetest/colors.txt";
		if (file_exists(check))
			return check;
	}
#endif

	constexpr bool sharedir_valid = !(SHAREDIR[0] == '.' || !SHAREDIR[0]);
	if (sharedir_valid && file_exists(SHAREDIR "/colors.txt"))
		return SHAREDIR "/colors.txt";

	errorstream << "Warning: Falling back to using colors.txt from current directory." << std::endl;
	return "./colors.txt";
}

int main(int argc, char *argv[])
{
	const char *short_options = "hi:o:v";
	const static struct option long_options[] =
	{
		{"help", no_argument, 0, 'h'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{"bgcolor", required_argument, 0, 'b'},
		{"scalecolor", required_argument, 0, 's'},
		{"origincolor", required_argument, 0, 'r'},
		{"playercolor", required_argument, 0, 'p'},
		{"draworigin", no_argument, 0, 'R'},
		{"drawplayers", no_argument, 0, 'P'},
		{"drawscale", no_argument, 0, 'S'},
		{"drawalpha", no_argument, 0, 'e'},
		{"noshading", no_argument, 0, 'H'},
		{"backend", required_argument, 0, 'd'},
		{"geometry", required_argument, 0, 'g'},
		{"extent", no_argument, 0, 'E'},
		{"min-y", required_argument, 0, 'a'},
		{"max-y", required_argument, 0, 'c'},
		{"zoom", required_argument, 0, 'z'},
		{"colors", required_argument, 0, 'C'},
		{"scales", required_argument, 0, 'f'},
		{"noemptyimage", no_argument, 0, 'n'},
		{"exhaustive", required_argument, 0, 'j'},
		{"dumpblock", required_argument, 0, 'k'},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	configure_log_streams(false);

	std::string input;
	std::string output;
	std::string colors;
	bool onlyPrintExtent = false;
	BlockPos dumpblock(INT16_MIN);

	TileGenerator generator;
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break; // done

		switch (c) {
			case 'h':
				usage();
				return 0;
			case 'i':
				input = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'b':
				generator.setBgColor(optarg);
				break;
			case 's':
				generator.setScaleColor(optarg);
				break;
			case 'r':
				generator.setOriginColor(optarg);
				break;
			case 'p':
				generator.setPlayerColor(optarg);
				break;
			case 'R':
				generator.setDrawOrigin(true);
				break;
			case 'P':
				generator.setDrawPlayers(true);
				break;
			case 'S':
				generator.setDrawScale(true);
				break;
			case 'e':
				generator.setDrawAlpha(true);
				break;
			case 'E':
				onlyPrintExtent = true;
				break;
			case 'H':
				generator.setShading(false);
				break;
			case 'd':
				generator.setBackend(optarg);
				break;
			case 'a':
				generator.setMinY(stoi(optarg));
				break;
			case 'c':
				generator.setMaxY(stoi(optarg));
				break;
			case 'g': {
					std::istringstream geometry(optarg);
					int x, y, w, h;
					char c;
					geometry >> x >> c >> y >> w >> h;
					if (geometry.fail() || c != ':' || w < 1 || h < 1) {
						usage();
						return 1;
					}
					generator.setGeometry(x, y, w, h);
				}
				break;
			case 'f': {
					uint flags = 0;
					if (strchr(optarg, 't'))
						flags |= SCALE_TOP;
					if (strchr(optarg, 'b'))
						flags |= SCALE_BOTTOM;
					if (strchr(optarg, 'l'))
						flags |= SCALE_LEFT;
					if (strchr(optarg, 'r'))
						flags |= SCALE_RIGHT;
					generator.setScales(flags);
				}
				break;
			case 'z':
				generator.setZoom(stoi(optarg));
				break;
			case 'C':
				colors = optarg;
				break;
			case 'n':
				generator.setDontWriteEmpty(true);
				break;
			case 'j': {
					int mode = EXH_AUTO;
					if (!strcmp(optarg, "never"))
						mode = EXH_NEVER;
					else if (!strcmp(optarg, "y"))
						mode = EXH_Y;
					else if (!strcmp(optarg, "full"))
						mode = EXH_FULL;
					generator.setExhaustiveSearch(mode);
				}
				break;
			case 'k': {
				std::istringstream iss(optarg);
				char c, c2;
				iss >> dumpblock.x >> c >> dumpblock.y >> c2 >> dumpblock.z;
				if (iss.fail() || c != ',' || c2 != ',') {
					usage();
					return 1;
				}
				break;
			}
			case 'v':
				configure_log_streams(true);
				break;
			default:
				return 1;
		}
	}

	const bool need_output = !onlyPrintExtent && dumpblock.x == INT16_MIN;
	if (input.empty() || (need_output && output.empty())) {
		usage();
		return 0;
	}

	try {
		if (onlyPrintExtent) {
			generator.printGeometry(input);
			return 0;
		} else if (dumpblock.x != INT16_MIN) {
			generator.dumpBlock(input, dumpblock);
			return 0;
		}

		if(colors.empty())
			colors = search_colors(input);
		generator.parseColorsFile(colors);
		generator.generate(input, output);

	} catch (const std::exception &e) {
		errorstream << "Exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
