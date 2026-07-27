/// @file
/// @brief Conversion between narrow and wide character encodings.
///
/// On Windows the wide forms are UTF-16 and the narrow forms are the active code
/// page; elsewhere the conversion runs under a temporarily installed UTF-8 locale
/// so that results do not depend on whatever locale the host program has set.

module;
// LC_ALL and the other locale category macros are macros, so they cannot arrive
// through `import std;` and must be included in the global module fragment.
#ifndef _WIN32
#include <clocale>
#endif

export module cli11:encoding;

import std;

namespace cli::detail
{

#ifndef _WIN32

    /// @brief Installs the first available UTF-8 locale for every category.
    ///
    /// @throws std::runtime_error If no UTF-8 locale is available.
    auto set_unicode_locale() -> void
    {
        static constexpr std::array<const char *, 2> unicode_locales {{"C.UTF-8", ".UTF-8"}};
        for (const auto &locale_name : unicode_locales)
        {
            if (std::setlocale(LC_ALL, locale_name) != nullptr)
            {
                return;
            }
        }
        throw std::runtime_error("cli::narrow: could not set locale to C.UTF-8");
    }

    /// @brief Runs a closure when it goes out of scope.
    ///
    /// @tparam F Any callable invocable with no arguments.
    template <typename F> struct scope_guard_t
    {
            /// @brief The closure to run on destruction.
            F closure;

            /// @brief Takes ownership of the closure.
            ///
            /// @param closure_ The closure to run on destruction.
            explicit scope_guard_t(F closure_) : closure(closure_)
            {
            }

            /// @brief Runs the closure.
            ~scope_guard_t()
            {
                closure();
            }
    };

    /// @brief Builds a @ref scope_guard_t deducing the closure type.
    ///
    /// @tparam F Any callable invocable with no arguments.
    /// @param closure The closure to run when the guard is destroyed.
    /// @return A guard owning @p closure.
    template <typename F> [[nodiscard]] auto scope_guard(F &&closure) -> scope_guard_t<F>
    {
        return scope_guard_t<F> {std::forward<F>(closure)};
    }

#endif

    /// @brief Converts a null-terminated wide string to a narrow string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @param[in] str_size Unused; the conversion stops at the null terminator.
    /// @return The converted string.
    /// @throws std::runtime_error If the input is not convertible.
    auto narrow_impl(const wchar_t *str, [[maybe_unused]] std::size_t str_size) -> std::string
    {
        std::mbstate_t state = std::mbstate_t();
        const wchar_t *it = str;

#ifndef _WIN32
        const std::string old_locale = std::setlocale(LC_ALL, nullptr);
        auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
        set_unicode_locale();
#endif

        const std::size_t new_size = std::wcsrtombs(nullptr, &it, 0, &state);
        if (new_size == static_cast<std::size_t>(-1))
        {
            throw std::runtime_error(
                std::format("cli::narrow: conversion error in std::wcsrtombs at offset {}", it - str));
        }
        std::string result(new_size, '\0');
        std::wcsrtombs(result.data(), &str, new_size, &state);
        return result;
    }

    /// @brief Converts a null-terminated narrow string to a wide string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @param[in] str_size Unused; the conversion stops at the null terminator.
    /// @return The converted string.
    /// @throws std::runtime_error If the input is not convertible.
    auto widen_impl(const char *str, [[maybe_unused]] std::size_t str_size) -> std::wstring
    {
        std::mbstate_t state = std::mbstate_t();
        const char *it = str;

#ifndef _WIN32
        const std::string old_locale = std::setlocale(LC_ALL, nullptr);
        auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
        set_unicode_locale();
#endif

        const std::size_t new_size = std::mbsrtowcs(nullptr, &it, 0, &state);
        if (new_size == static_cast<std::size_t>(-1))
        {
            throw std::runtime_error(
                std::format("cli::widen: conversion error in std::mbsrtowcs at offset {}", it - str));
        }
        std::wstring result(new_size, L'\0');
        std::mbsrtowcs(result.data(), &str, new_size, &state);
        return result;
    }

} // namespace cli::detail

export namespace cli
{

    /// @brief Converts a wide string to a narrow string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @param[in] str_size Unused; the conversion stops at the null terminator.
    /// @return The converted string.
    auto narrow(const wchar_t *str, std::size_t str_size) -> std::string
    {
        return detail::narrow_impl(str, str_size);
    }

    /// @brief Converts a wide string to a narrow string.
    ///
    /// @param[in] str The string to convert.
    /// @return The converted string.
    auto narrow(const std::wstring &str) -> std::string
    {
        return detail::narrow_impl(str.data(), str.size());
    }

    /// @brief Converts a null-terminated wide string to a narrow string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @return The converted string.
    auto narrow(const wchar_t *str) -> std::string
    {
        return detail::narrow_impl(str, std::wcslen(str));
    }

    /// @brief Converts a wide string view to a narrow string.
    ///
    /// @param[in] str The string to convert.
    /// @warning The conversion reads to a null terminator, so @p str must be
    /// null-terminated. See the notes accompanying this refactor.
    /// @return The converted string.
    auto narrow(std::wstring_view str) -> std::string
    {
        return detail::narrow_impl(str.data(), str.size());
    }

    /// @brief Converts a narrow string to a wide string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @param[in] str_size Unused; the conversion stops at the null terminator.
    /// @return The converted string.
    auto widen(const char *str, std::size_t str_size) -> std::wstring
    {
        return detail::widen_impl(str, str_size);
    }

    /// @brief Converts a narrow string to a wide string.
    ///
    /// @param[in] str The string to convert.
    /// @return The converted string.
    auto widen(const std::string &str) -> std::wstring
    {
        return detail::widen_impl(str.data(), str.size());
    }

    /// @brief Converts a null-terminated narrow string to a wide string.
    ///
    /// @param[in] str The null-terminated string to convert.
    /// @return The converted string.
    auto widen(const char *str) -> std::wstring
    {
        return detail::widen_impl(str, std::strlen(str));
    }

    /// @brief Converts a narrow string view to a wide string.
    ///
    /// @param[in] str The string to convert.
    /// @warning The conversion reads to a null terminator, so @p str must be
    /// null-terminated. See the notes accompanying this refactor.
    /// @return The converted string.
    auto widen(std::string_view str) -> std::wstring
    {
        return detail::widen_impl(str.data(), str.size());
    }

    /// @brief Builds a filesystem path from a narrow string.
    ///
    /// On Windows the string is widened first, so that paths outside the active
    /// code page survive the round trip.
    ///
    /// @param[in] str The path, as a narrow string.
    /// @return The corresponding path.
    auto to_path(std::string_view str) -> std::filesystem::path
    {
        return std::filesystem::path {
#ifdef _WIN32
            widen(str)
#else
            str
#endif
        };
    }

} // namespace cli
