// MIT License
#ifndef LIPH_ENDIAN_HPP
#define LIPH_ENDIAN_HPP

#include <cstddef>
#include <limits>
#include <type_traits>

/*
 *  gcc 8.1 optimizes all of these to simple MOVs or BSWAPs.
 *  clang 9.0 optimizes the to_X_endian functions but not the from_X_endian functions.
 *
 *  Example usage:
 *
 *   // read 32-bit unsigned int from file, increment value, and write it back out
 *   unsigned char bytes[4];
 *   std::fstream file("filename", std::ios::binary);
 *   file.read(reinterpret_cast<char*>(bytes), 4);
 *   std::uint32_t value = from_little_endian<std::uint32_t>(bytes);
 *   to_little_endian(bytes, value + 1);
 *   file.write(reinterpret_cast<char*>(bytes), 4);
 */
template<typename Integer>
unsigned char* to_little_endian(unsigned char *dest, Integer src) {
    static_assert(std::is_integral_v<Integer>);

    constexpr int char_bits = std::numeric_limits<unsigned char>::digits;
    constexpr unsigned char mask = std::numeric_limits<unsigned char>::max();

    std::make_unsigned_t<Integer> s = src;

    for(std::size_t i = 0; i < sizeof(Integer); ++i) {
        dest[i] = (s >> (i * char_bits)) & mask;
    }

    return dest;
}

template<typename Integer>
unsigned char* to_big_endian(unsigned char *dest, Integer src) {
    static_assert(std::is_integral_v<Integer>);

    constexpr int char_bits = std::numeric_limits<unsigned char>::digits;
    constexpr unsigned char mask = std::numeric_limits<unsigned char>::max();

    std::make_unsigned_t<Integer> s = src;

    for(std::size_t i = 0; i < sizeof(Integer); ++i) {
        dest[sizeof(Integer) - i - 1] = (s >> (i * char_bits)) & mask;
    }

    return dest;
}

template<typename Integer>
Integer from_little_endian(const unsigned char *src) {
    static_assert(std::is_integral_v<Integer>);

    constexpr int char_bits = std::numeric_limits<unsigned char>::digits;
    std::make_unsigned_t<Integer> dest = 0;

    for(std::size_t i = 0; i < sizeof(Integer); ++i) {
        dest |= static_cast<Integer>(src[i]) << (i * char_bits);
    }

    return dest;
}

template<typename Integer>
Integer from_big_endian(const unsigned char *src) {
    static_assert(std::is_integral_v<Integer>);

    constexpr int char_bits = std::numeric_limits<unsigned char>::digits;
    std::make_unsigned_t<Integer> dest = 0;

    for(std::size_t i = 0; i < sizeof(Integer); ++i) {
        dest |= static_cast<Integer>(src[sizeof(Integer) - i - 1]) << (i * char_bits);
    }

    return dest;
}

#endif