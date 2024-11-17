#pragma once

#if !__cpp_concepts >= 201907L
    #error "This library requires C++20"
#endif

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "fatal_error.hh"

#ifndef METASTR_N_STL_SUPPORT
    #include <string>
    #include <string_view>
#endif

namespace metastr {

// clang-format off
template<typename Char>
concept is_char =
    std::is_same_v<::std::remove_cv_t<Char>, char>
    || std::is_same_v<::std::remove_cv_t<Char>, wchar_t>
#if __cpp_char8_t >= 201811L
    || ::std::is_same_v<::std::remove_cv_t<Char>, char8_t>
#endif
    || std::is_same_v<::std::remove_cv_t<Char>, char16_t>
    || std::is_same_v<::std::remove_cv_t<Char>, char32_t>;
// clang-format on

namespace details::transcoding {

constexpr auto LEAD_SURROGATE_MIN = char16_t{0xd800u};
constexpr auto LEAD_SURROGATE_MAX = char16_t{0xdbffu};
constexpr auto TRAIL_SURROGATE_MIN = char16_t{0xdc00u};
constexpr auto TRAIL_SURROGATE_MAX = char16_t{0xdfffu};
// LEAD_SURROGATE_MIN - (0x10000 >> 10)
constexpr auto LEAD_OFFSET = char16_t{0xd7c0u};
// 0x10000u - (LEAD_SURROGATE_MIN << 10) - TRAIL_SURROGATE_MIN
constexpr auto SURROGATE_OFFSET = char32_t{0xfca02400u};
// Maximum valid value for a Unicode code point
constexpr auto CODE_POINT_MAX = char32_t{0x0010ffffu};

/* Assume sizeof(char) == 1
 * Assume encoding of char8_t and char is utf-8
 * Despite encoding of char sometimes is not utf-8 on windows
 *
 * So, do NOT use `char`
 */
template<typename Char>
concept is_utf8 =
#if __cpp_char8_t >= 201811L
    ::std::is_same_v<::std::remove_cv_t<Char>, char8_t> ||  // unprintable utf-8 type
#endif
    ::std::is_same_v<::std::remove_cv_t<Char>, char>;  // printable utf-8 type

/* Assume encoding of char16_t and
 * encoding of wchar_t(on windows) is utf-16
 *
 * So, do NOT use wchar_t
 */
template<typename Char>
concept is_utf16 =
#ifdef _WIN32
    ::std::is_same_v<::std::remove_cv_t<Char>, wchar_t> ||
#endif
    ::std::is_same_v<::std::remove_cv_t<Char>, char16_t>;

/* Assume encoding of char32_t and
 * wchar(not on windows) is utf-32
 *
 * So, do NOT use wchar_t
 */
template<typename Char>
concept is_utf32 =
#ifndef _WIN32
    ::std::is_same_v<::std::remove_cv_t<Char>, wchar_t> ||
#endif
    ::std::is_same_v<::std::remove_cv_t<Char>, char32_t>;

}  // namespace details::transcoding

/* class metastr
 *
 * A string literal that can be used in template.
 *
 * This class just for compile-time using, and to support it, functions/methods in
 * namespace metastr usually return a new object of class metastr.
 *
 * if you have another runtime requirement, please convert metastr to
 * std::string or std::string_view.
 */
template<is_char Char, ::std::size_t N>
struct metastr {
    using char_type = Char;
    static constexpr auto len{N};
    Char str[N]{};

    constexpr metastr() noexcept = delete;

    constexpr metastr(Char const (&arr)[N]) noexcept {
        ::std::copy(arr, arr + N - 1, str);
    }

    template<is_char Char_r, ::std::size_t N_r>
    constexpr bool operator==(Char_r const (&other)[N_r]) const noexcept {
        if constexpr (N <= N_r) {
            if (!::std::equal(this->str, this->str + N - 2, other)) {
                return false;
            }
            for (::std::size_t i{N - 1}; i < N_r; ++i) {
                if (other[i] != '\0') {
                    return false;
                }
            }
            return true;
        } else {
            if (!::std::equal(other, other + N_r - 2, this->str)) {
                return false;
            }
            for (::std::size_t i{N_r - 1}; i < N; ++i) {
                if (this->str[i] != '\0') {
                    return false;
                }
            }
            return true;
        }
    }

    template<is_char Char_r, ::std::size_t N_r>
    constexpr bool operator==(metastr<Char_r, N_r> const& other) const noexcept {
        return *this == other.str;
    }

#ifndef METASTR_N_STL_SUPPORT
    template<is_char Char_r>
    constexpr bool operator==(::std::basic_string_view<Char_r> const& other) const noexcept {
        if (N <= other.size()) {
            if (!::std::equal(this->str, this->str + N - 2, other.begin())) {
                return false;
            }
            for (::std::size_t i{N - 1}; i < other.size(); ++i) {
                if (other[i] != '\0') {
                    return false;
                }
            }
            return true;
        } else {
            if (!::std::equal(other.begin(), other.end() - 1, this->str)) {
                return false;
            }
            for (::std::size_t i{other.size()}; i < N; ++i) {
                if (this->str[i] != '\0') {
                    return false;
                }
            }
            return true;
        }
    }

    template<is_char Char_r>
    constexpr bool operator==(::std::basic_string<Char_r> const other) const noexcept {
        return *this == ::std::basic_string_view<Char_r>{other};
    }

    constexpr operator ::std::basic_string<Char>() const noexcept {
        return ::std::basic_string<Char>{str, N - 1};
    }

    constexpr operator ::std::basic_string_view<Char>() const noexcept {
        return ::std::basic_string_view<Char>{str, N - 1};
    }
#endif
};

namespace details::transcoding {

template<details::transcoding::is_utf32 Char, ::std::size_t N, details::transcoding::is_utf8 u8_type>
constexpr auto utf32to8(metastr<Char, N> const& u32str) noexcept {
    constexpr u8_type tmp_[N * 4 - 3]{};
    metastr res{tmp_};

    auto index = ::std::size_t{};
    for (auto u32chr : u32str.str) {
#ifndef NDEBUG
        if (u32chr > details::transcoding::CODE_POINT_MAX
            || u32chr >= details::transcoding::LEAD_SURROGATE_MIN
            && u32chr <= details::transcoding::TRAIL_SURROGATE_MAX) [[unlikely]]
        {
            fatal_error::terminate();
        }
#endif  // NDEBUG

        if (u32chr < 0x80) {
            res.str[index++] = static_cast<u8_type>(u32chr);
        } else if (u32chr < 0x800) {
            res.str[index++] = static_cast<u8_type>((u32chr >> 6) | 0xc0);
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        } else if (u32chr < 0x10000) {
            res.str[index++] = static_cast<u8_type>((u32chr >> 12) | 0xe0);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 6) & 0x3f) | 0x80);
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        } else {
            res.str[index++] = static_cast<u8_type>((u32chr >> 18) | 0xf0);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 12) & 0x3f) | 0x80);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 6) & 0x3f));
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        }
    }

    return res;
}

template<details::transcoding::is_utf16 Char, ::std::size_t N, details::transcoding::is_utf8 u8_type>
constexpr auto utf16to8(metastr<Char, N> const& u16str) noexcept {
    constexpr u8_type tmp_[4 * N - 3]{};
    metastr res{tmp_};

    auto index = ::std::size_t{};
    for (::std::size_t i{}; i < N;) {
        auto u32chr = static_cast<char32_t>(u16str.str[i++] & 0xffff);
        // clang-format off
#ifndef NDEBUG
        if (u32chr >= details::transcoding::TRAIL_SURROGATE_MIN
            && u32chr <= details::transcoding::TRAIL_SURROGATE_MAX) [[unlikely]]
        {
            fatal_error::terminate();
        }
#endif  // NDEBUG
        if (u32chr >= details::transcoding::LEAD_SURROGATE_MIN
            && u32chr <= details::transcoding::LEAD_SURROGATE_MAX)
        {
#ifndef NDEBUG
            if (i >= N) [[unlikely]] {
                fatal_error::terminate();
            }
#endif  // NDEBUG
            auto const trail_surrogate = static_cast<char32_t>(u16str.str[i++] & 0xffff);
#ifndef NDEBUG
            if (trail_surrogate < details::transcoding::TRAIL_SURROGATE_MIN
                || trail_surrogate > details::transcoding::TRAIL_SURROGATE_MAX) [[unlikely]]
            {
                fatal_error::terminate();
            }
#endif  // NDEBUG
            u32chr = (u32chr << 10) + trail_surrogate + details::transcoding::SURROGATE_OFFSET;
        }
        // clang-format on

        if (u32chr < 0x80) {
            res.str[index++] = static_cast<u8_type>(u32chr);
        } else if (u32chr < 0x800) {
            res.str[index++] = static_cast<u8_type>((u32chr >> 6) | 0xc0);
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        } else if (u32chr < 0x10000) {
            res.str[index++] = static_cast<u8_type>((u32chr >> 12) | 0xe0);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 6) & 0x3f) | 0x80);
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        } else {
            res.str[index++] = static_cast<u8_type>((u32chr >> 18) | 0xf0);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 12) & 0x3f) | 0x80);
            res.str[index++] = static_cast<u8_type>(((u32chr >> 6) & 0x3f));
            res.str[index++] = static_cast<u8_type>((u32chr & 0x3f) | 0x80);
        }
    }
    return res;
}

}  // namespace details::transcoding

/* Convert a metastr to another encoding.
 * Assume char, char8_t -> utf-8
 *        char16_t, wchar_t(Windows) -> utf-16
 *        char32_t, wchar_t(not Windows) -> utf-32
 *
 * Despite on linux, MacOS and Windows(USA), char's encoding is `utf-8`
 * but on windows, char's encoding relys on the system.
 * So, I strongly suggest you do NOT use type `char` and `wchar_t` derectly.
 *
 * And do NOT try to use char8_t with utf-32 encoding etc. (Thank god,
 * compiler has some checks that can avoid some mistakes like this)
 */
template<is_char Char_r, is_char Char, ::std::size_t N>
constexpr auto code_cvt(metastr<Char, N> const& str) noexcept {
    // clang-format off
    if constexpr (
        details::transcoding::is_utf8<Char> && details::transcoding::is_utf8<Char_r>
        || details::transcoding::is_utf16<Char> && details::transcoding::is_utf16<Char_r>
        || details::transcoding::is_utf32<Char> && details::transcoding::is_utf32<Char_r>
    ) {
        Char_r tmp_[N]{};
        ::std::copy(str.str, str.str + N - 1, tmp_);
        return metastr{tmp_};
    }
    // clang-format on
    else if constexpr (details::transcoding::is_utf32<Char> && details::transcoding::is_utf8<Char_r>) {
        return details::transcoding::utf32to8<Char, N, Char_r>(str);
    } else if constexpr (details::transcoding::is_utf16<Char> && details::transcoding::is_utf8<Char_r>) {
        return details::transcoding::utf16to8<Char, N, Char_r>(str);
    }
}

namespace details {

template<typename>
constexpr bool is_metastr_ = false;

template<is_char Char, ::std::size_t N>
constexpr bool is_metastr_<metastr<Char, N>> = true;

}  // namespace details

template<typename T>
concept is_metastr = details::is_metastr_<::std::remove_cvref_t<T>>;

namespace details {

template<typename>
constexpr bool is_c_str_ = false;

template<is_char Char, ::std::size_t N>
constexpr bool is_c_str_<Char[N]> = true;

template<typename T>
concept is_c_str = is_c_str_<::std::remove_cvref_t<T>>;

template<typename T>
concept can_concat = is_metastr<T> || is_c_str<T>;

}  // namespace details

template<details::can_concat... T>
[[nodiscard]] constexpr auto concat(T const&... strs) noexcept {
    return concat([strs] {
        if constexpr (is_metastr<T>) {
            return strs;
        } else {  // details::is_c_str<T>
            return metastr{strs};
        }
    }()...);
}

template<is_metastr Str1, is_metastr... Strs>
    requires (::std::is_same_v<typename Str1::char_type, typename Strs::char_type> && ...)
[[nodiscard]] constexpr auto concat(Str1 const& str1, Strs const&... strs) noexcept {
    constexpr typename Str1::char_type tmp_[Str1::len + (Strs::len + ...) - sizeof...(Strs)]{};
    auto res = metastr{tmp_};
    constexpr decltype(Str1::len) lens[]{Str1::len - 1, (Strs::len - 1)...};
    ::std::size_t index{}, offset{};
    ::std::copy(str1.str, str1.str + Str1::len - 1, res.str);
    (::std::copy(strs.str, strs.str + Strs::len - 1, (offset += lens[index++], res.str + offset)), ...);
    return res;
}

}  // namespace metastr
