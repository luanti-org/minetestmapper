#include <iostream>

#include "log.h"

StreamProxy errorstream(nullptr);
StreamProxy verbosestream(nullptr);

void configure_log_streams(bool verbose)
{
	errorstream << std::flush;
	verbosestream << std::flush;

	errorstream = std::cerr.good() ? &std::cerr : nullptr;
	// std::clog does not automatically flush
	verbosestream = (verbose && std::clog.good()) ? &std::clog : nullptr;
}
