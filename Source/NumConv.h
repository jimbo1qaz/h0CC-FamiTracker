/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2017 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/


#pragma once

#include <string_view>
#include <cstdint>
#include <optional>
#include <limits>

// // // locale-independent number conversion routines

namespace conv {

constexpr unsigned from_digit(char ch) noexcept {
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	return (unsigned)-1;
}

namespace details {

template <typename T>
constexpr std::optional<T>
to_unsigned(std::string_view sv, unsigned radix) noexcept {
	static_assert(sizeof(T) < sizeof(uintmax_t));
	if (!sv.empty()) {
		uintmax_t x = 0uLL;
		for (auto ch : sv) {
			unsigned digit = from_digit(ch);
			if (digit == (unsigned)-1 || digit >= radix)
				return std::nullopt;
			x = x * radix + digit;
			if (x > std::numeric_limits<T>::max())
				return std::nullopt;
		}
		return static_cast<T>(x);
	}
	return std::nullopt;
}

template <typename T, typename U>
constexpr std::optional<T>
to_signed(std::string_view sv, unsigned radix) noexcept {
	bool negative = false;
	if (!sv.empty()) {
		if (sv.front() == '-') {
			negative = true;
			sv.remove_prefix(1);
		}
		else if (sv.front() == '+')
			sv.remove_prefix(1);
	}

	if (auto px = details::to_unsigned<U>(sv, radix)) {
		auto x = static_cast<T>(*px);
		if (negative) {
			if (*px <= static_cast<U>(std::numeric_limits<T>::min()))
				return -x;
		}
		else {
			if (*px <= static_cast<U>(std::numeric_limits<T>::max()))
				return x;
		}
	}
	return std::nullopt;
}

} // namespace details

constexpr auto to_uint8(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_unsigned<uint8_t>(sv, radix);
}

constexpr auto to_uint16(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_unsigned<uint16_t>(sv, radix);
}

constexpr auto to_uint32(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_unsigned<uint32_t>(sv, radix);
}

constexpr auto to_int8(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_signed<int8_t, uint8_t>(sv, radix);
}

constexpr auto to_int16(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_signed<int16_t, uint16_t>(sv, radix);
}

constexpr auto to_int32(std::string_view sv, unsigned radix = 10) noexcept {
	return details::to_signed<int32_t, uint32_t>(sv, radix);
}



namespace details {

template <typename T, unsigned Radix>
constexpr std::size_t max_places() noexcept {
	std::size_t e = 1; // for minus sign
	auto x = std::numeric_limits<T>::max();
	while (x) {
		x /= Radix;
		++e;
	}
	return e;
}

} // namespace details

template <typename T>
constexpr char to_digit(T x) noexcept {
	if (x >= T(0)) {
		if (x < T(10))
			return static_cast<char>('0' + x);
		if (x < T(36))
			return static_cast<char>('A' + x - 10);
	}
	return '\0';
}

namespace details {

template <unsigned Radix>
struct str_buf_ {
	static constexpr std::size_t maxlen = details::max_places<uintmax_t, Radix>();
	static thread_local char data[maxlen + 1];
};

template <unsigned Radix>
thread_local char str_buf_<Radix>::data[maxlen + 1] = { };

template <unsigned Radix>
char *from_uint_impl(uintmax_t x, unsigned places) noexcept {
	char *it = std::end(details::str_buf_<Radix>::data) - 1;

	if (places == (unsigned)-1) {
		if (!x) {
			*--it = to_digit(0);
			return it;
		}
		while (x) {
			*--it = to_digit(x % Radix);
			x /= Radix;
		}
		return it;
	}

	if (places >= details::str_buf_<Radix>::maxlen)
		places = details::str_buf_<Radix>::maxlen - 1;
	while (places--) {
		*--it = to_digit(x % Radix);
		x /= Radix;
	}
	return it;
}

template <unsigned Radix>
std::string from_uint(uintmax_t x, unsigned places) noexcept {
	return from_uint_impl<Radix>(x, places);
}

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif
template <unsigned Radix>
std::string from_int(intmax_t x, unsigned places) noexcept {
	if (x >= 0)
		return from_uint<Radix>(x, places);
	char *it = from_uint_impl<Radix>(-static_cast<uintmax_t>(x), places);
	*--it = '-';
	return it;
}
#ifdef _MSC_VER
#pragma warning (pop)
#endif

} // namespace details

auto from_uint(uintmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_uint<10>(x, places);
}

auto from_uint_bin(uintmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_uint<2>(x, places);
}

auto from_uint_oct(uintmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_uint<8>(x, places);
}

auto from_uint_hex(uintmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_uint<16>(x, places);
}

auto from_int(intmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_int<10>(x, places);
}

auto from_int_bin(intmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_int<2>(x, places);
}

auto from_int_oct(intmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_int<8>(x, places);
}

auto from_int_hex(intmax_t x, unsigned places = (unsigned)-1) noexcept {
	return details::from_int<16>(x, places);
}

} // namespace conv