module;

#ifndef _WIN32
#include <clocale>
#endif

export module cli11:encoding;

import std;

namespace CLI::detail {

#ifndef _WIN32
void set_unicode_locale() {
    static constexpr std::array<const char *, 2> unicode_locales{{"C.UTF-8", ".UTF-8"}};
    for (const auto &locale_name : unicode_locales) {
        if (std::setlocale(LC_ALL, locale_name) != nullptr) {
            return;
        }
    }
    throw std::runtime_error("CLI::narrow: could not set locale to C.UTF-8");
}

template <typename F>
struct scope_guard_t {
    F closure;
    explicit scope_guard_t(F closure_) : closure(closure_) {}
    ~scope_guard_t() { closure(); }
};

template <typename F>
[[nodiscard]] scope_guard_t<F> scope_guard(F &&closure) {
    return scope_guard_t<F>{std::forward<F>(closure)};
}
#endif

std::string narrow_impl(const wchar_t *str, std::size_t str_size) {
    (void)str_size;
    std::mbstate_t state = std::mbstate_t();
    const wchar_t *it = str;

#ifndef _WIN32
    std::string old_locale = std::setlocale(LC_ALL, nullptr);
    auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
    set_unicode_locale();
#endif

    std::size_t new_size = std::wcsrtombs(nullptr, &it, 0, &state);
    if (new_size == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("CLI::narrow: conversion error in std::wcsrtombs at offset " +
                                 std::to_string(it - str));
    }
    std::string result(new_size, '\0');
    std::wcsrtombs(result.data(), &str, new_size, &state);
    return result;
}

std::wstring widen_impl(const char *str, std::size_t str_size) {
    (void)str_size;
    std::mbstate_t state = std::mbstate_t();
    const char *it = str;

#ifndef _WIN32
    std::string old_locale = std::setlocale(LC_ALL, nullptr);
    auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
    set_unicode_locale();
#endif

    std::size_t new_size = std::mbsrtowcs(nullptr, &it, 0, &state);
    if (new_size == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("CLI::widen: conversion error in std::mbsrtowcs at offset " +
                                 std::to_string(it - str));
    }
    std::wstring result(new_size, L'\0');
    std::mbsrtowcs(result.data(), &str, new_size, &state);
    return result;
}

}  // namespace CLI::detail

export namespace CLI {

std::string narrow(const wchar_t *str, std::size_t str_size) { return detail::narrow_impl(str, str_size); }
std::string narrow(const std::wstring &str) { return detail::narrow_impl(str.data(), str.size()); }
std::string narrow(const wchar_t *str) { return detail::narrow_impl(str, std::wcslen(str)); }
std::string narrow(std::wstring_view str) { return detail::narrow_impl(str.data(), str.size()); }

std::wstring widen(const char *str, std::size_t str_size) { return detail::widen_impl(str, str_size); }
std::wstring widen(const std::string &str) { return detail::widen_impl(str.data(), str.size()); }
std::wstring widen(const char *str) { return detail::widen_impl(str, std::strlen(str)); }
std::wstring widen(std::string_view str) { return detail::widen_impl(str.data(), str.size()); }

std::filesystem::path to_path(std::string_view str) {
    return std::filesystem::path{
#ifdef _WIN32
        widen(str)
#else
        str
#endif
    };
}

}  // namespace CLI
