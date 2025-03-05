#pragma once

#include <iostream>
#include <utility>

// Forwards to an ostream, optionally
class StreamProxy {
public:
	StreamProxy(std::ostream *os) : m_os(os) {}

	template<typename T>
	StreamProxy &operator<<(T &&arg)
	{
		if (m_os)
			*m_os << std::forward<T>(arg);
		return *this;
	}

	StreamProxy &operator<<(std::ostream &(*func)(std::ostream&))
	{
		if (m_os)
			*m_os << func;
		return *this;
	}

private:
	std::ostream *m_os;
};

/// Error and warning output, forwards to std::cerr
extern StreamProxy errorstream;
/// Verbose output, might forward to std::cerr
extern StreamProxy verbosestream;

/**
 * Configure log streams defined in this file.
 * @param verbose enable verbose output
 * @note not thread-safe!
 */
void configure_log_streams(bool verbose);
