#pragma once

#include <string>

// Define custom char traits since std::char_traits<unsigend char> is not part of C++ standard
struct uchar_traits : std::char_traits<char>
{
	using super = std::char_traits<char>;
	using char_type = unsigned char;

	static void assign(char_type& c1, const char_type& c2) noexcept {
		c1 = c2;
	}
	static char_type* assign(char_type* ptr, std::size_t count, char_type c2) {
		return reinterpret_cast<char_type*>(
			super::assign(reinterpret_cast<char*>(ptr), count, static_cast<char>(c2)));
	}

	static char_type* move(char_type* dest, const char_type* src, std::size_t count) {
		return reinterpret_cast<char_type*>(
			super::move(reinterpret_cast<char*>(dest), reinterpret_cast<const char*>(src), count));
	}

	static char_type* copy(char_type* dest, const char_type* src, std::size_t count) {
		return reinterpret_cast<char_type*>(
			super::copy(reinterpret_cast<char*>(dest), reinterpret_cast<const char*>(src), count));
	}

	static int compare(const char_type* s1, const char_type* s2, std::size_t count) {
		return super::compare(reinterpret_cast<const char*>(s1), reinterpret_cast<const char*>(s2), count);
	}

	static char_type to_char_type(int_type c) noexcept {
		return static_cast<char_type>(c);
	}
};

typedef std::basic_string<unsigned char, uchar_traits> ustring;
typedef unsigned int uint;
typedef unsigned char u8;
