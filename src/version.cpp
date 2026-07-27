/// @file
/// @brief Compile-time version information for the cli11 library.
///
/// Every value in this partition is a constant expression, so all of them may
/// be used in `static_assert` and other compile-time contexts:
///
/// @code
/// static_assert(cli::version_major >= 2, "cli11 2.x or newer is required");
/// @endcode

export module cli11:version;

import std;

export namespace cli
{

    /// @brief Major component of the library version.
    constexpr int version_major = 2;

    /// @brief Minor component of the library version.
    constexpr int version_minor = 6;

    /// @brief Patch component of the library version.
    constexpr int version_patch = 2;

    /// @brief Full library version, formatted as `"major.minor.patch"`.
    ///
    /// @note This is a `std::string_view`, not a `const char *`. Copy-initialising
    /// a `std::string` from it (`std::string s = cli::version;`) will not compile,
    /// because the relevant `std::string` constructor is `explicit`. Use direct
    /// initialisation instead: `std::string s{cli::version};`.
    constexpr std::string_view version = "2.6.2";

    /// @brief Token kinds.
    ///
    /// @note Every enumerator here names a C++ keyword, so each carries a
    /// trailing underscore. Keywords are keyword tokens from translation phase 7
    /// and are never identifiers, so they cannot appear in enumerator position
    /// regardless of the enclosing scope.
    enum class token_t : std::uint8_t
    {
        short_,  ///< The `short` keyword.
        class_,  ///< The `class` keyword.
        return_, ///< The `return` keyword.
        while_   ///< The `while` keyword.
    };

} // namespace cli
