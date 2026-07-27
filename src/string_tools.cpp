/// @file
/// @brief Low-level string manipulation shared across the library.
///
/// Everything here is free-standing text processing: splitting, joining,
/// trimming, quote handling, escape encoding and decoding, and the paragraph
/// wrapper used by the help formatter. Nothing in this partition knows about
/// options or applications.
///
/// Most helpers are `constexpr`. The ones that are not depend on `std::locale`
/// or `std::format`, neither of which is usable during constant evaluation.

module;
// std::getenv, _dupenv_s and std::free come from the C library. `import std;`
// re-exports the functions, but the MSVC secure variants need the header.
#include <cstdlib>

export module cli11:string_tools;

import std;

export namespace cli
{

    namespace enums
    {

        /// @brief Streams any enumeration as its underlying integer value.
        ///
        /// @tparam T Any enumeration type.
        /// @param in The stream to write to.
        /// @param item The enumerator to write.
        /// @return @p in, for chaining.
        template <typename T>
            requires std::is_enum_v<T>
        auto operator<<(std::ostream &in, const T &item) -> std::ostream &
        {
            return in << +static_cast<std::underlying_type_t<T>>(item);
        }

    } // namespace enums

    using enums::operator<<;

} // namespace cli

namespace cli::detail
{

    /// @brief Characters that require a backslash escape.
    ///
    /// Index-aligned with @ref escaped_chars_code: the escape code for
    /// `escaped_chars[i]` is `escaped_chars_code[i]`.
    constexpr std::string_view escaped_chars = "\b\t\n\f\r\"\\";

    /// @brief Escape codes, index-aligned with @ref escaped_chars.
    constexpr std::string_view escaped_chars_code = "btnfr\"\\";

    /// @brief Characters that open a quoted or bracketed sequence.
    ///
    /// Index-aligned with @ref match_bracket_chars: the closing character for
    /// `bracket_chars[i]` is `match_bracket_chars[i]`.
    constexpr std::string_view bracket_chars = "\"'`[(<{";

    /// @brief Closing characters, index-aligned with @ref bracket_chars.
    constexpr std::string_view match_bracket_chars = "\"'`])>}";

    /// @brief Converts a single hexadecimal digit to its value.
    ///
    /// @param hc A hexadecimal digit, in either case.
    /// @return The digit's value, or a value greater than `0x0F` if @p hc is not
    /// a hexadecimal digit.
    constexpr auto hex_convert(char hc) -> std::uint32_t
    {
        int hcode {0};
        if (hc >= '0' && hc <= '9')
        {
            hcode = (hc - '0');
        }
        else if (hc >= 'A' && hc <= 'F')
        {
            hcode = (hc - 'A' + 10);
        }
        else if (hc >= 'a' && hc <= 'f')
        {
            hcode = (hc - 'a' + 10);
        }
        else
        {
            hcode = -1;
        }
        return static_cast<std::uint32_t>(hcode);
    }

    /// @brief Truncates a code unit to a `char` without sign-extension surprises.
    ///
    /// @param code The value to narrow.
    /// @return The low byte of @p code, as a `char`.
    constexpr auto make_char(std::uint32_t code) -> char
    {
        return static_cast<char>(static_cast<unsigned char>(code));
    }

    /// @brief Appends a Unicode code point to a string as UTF-8.
    ///
    /// @param[in,out] str The string to append to.
    /// @param[in] code The code point to encode.
    /// @throws std::invalid_argument If @p code is a surrogate.
    constexpr auto append_codepoint(std::string &str, std::uint32_t code) -> void
    {
        if (code < 0x80)
        {
            str.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            str.push_back(make_char(0xC0 | code >> 6));
            str.push_back(make_char(0x80 | (code & 0x3F)));
        }
        else if (code < 0x10000)
        {
            if (0xD800 <= code && code <= 0xDFFF)
            {
                throw std::invalid_argument("[0xD800, 0xDFFF] are not valid UTF-8.");
            }
            str.push_back(make_char(0xE0 | code >> 12));
            str.push_back(make_char(0x80 | (code >> 6 & 0x3F)));
            str.push_back(make_char(0x80 | (code & 0x3F)));
        }
        else if (code < 0x110000)
        {
            str.push_back(make_char(0xF0 | code >> 18));
            str.push_back(make_char(0x80 | (code >> 12 & 0x3F)));
            str.push_back(make_char(0x80 | (code >> 6 & 0x3F)));
            str.push_back(make_char(0x80 | (code & 0x3F)));
        }
    }

    /// @brief Strips a matched pair of @p key characters from both ends.
    ///
    /// @param[in,out] str The string to strip in place.
    /// @param[in] key The character to remove.
    /// @return A reference to @p str.
    constexpr auto remove_outer(std::string &str, char key) -> std::string &
    {
        if (str.length() > 1 && (str.front() == key))
        {
            if (str.front() == str.back())
            {
                str.pop_back();
                str.erase(str.begin(), str.begin() + 1);
            }
        }
        return str;
    }

    /// @brief Finds the closing quote of a string literal, honouring backslash escapes.
    ///
    /// @param str The text to scan.
    /// @param start Index of the opening quote.
    /// @param closure_char The quote character to match.
    /// @return Index of the closing quote, or `str.size()` if unterminated.
    constexpr auto close_string_quote(const std::string &str, std::size_t start, char closure_char) -> std::size_t
    {
        std::size_t loc {0};
        for (loc = start + 1; loc < str.size(); ++loc)
        {
            if (str[loc] == closure_char)
            {
                break;
            }
            if (str[loc] == '\\')
            {
                ++loc;
            }
        }
        return loc;
    }

    /// @brief Finds the closing quote of a literal string, ignoring escapes.
    ///
    /// @param str The text to scan.
    /// @param start Index of the opening quote.
    /// @param closure_char The quote character to match.
    /// @return Index of the closing quote, or `str.size()` if unterminated.
    constexpr auto close_literal_quote(const std::string &str, std::size_t start, char closure_char) -> std::size_t
    {
        const auto loc = str.find_first_of(closure_char, start + 1);
        return (loc != std::string::npos ? loc : str.size());
    }

    /// @brief Doubles every character of a bracketed sequence, marking it as nested.
    ///
    /// @param[in,out] str The string to rewrite in place; left alone unless it is
    /// bracketed with `[` and `]`.
    constexpr auto handle_secondary_array(std::string &str) -> void
    {
        if (str.size() >= 2 && str.front() == '[' && str.back() == ']')
        {
            std::string tstr {"[["};
            for (std::size_t ii = 1; ii < str.size(); ++ii)
            {
                tstr.push_back(str[ii]);
                tstr.push_back(str[ii]);
            }
            str = std::move(tstr);
        }
    }

} // namespace cli::detail

export namespace cli::detail
{

    /// @brief Upper bound on how many values a vector-like option may consume.
    constexpr int expected_max_vector_size {1 << 29};

    /// @brief Splits a string on a delimiter.
    ///
    /// An empty input yields a single empty element.
    ///
    /// @param s The string to split.
    /// @param delim The delimiter to split on.
    /// @return The resulting elements.
    ///
    /// @note This deliberately keeps the `std::getline` formulation rather than
    /// moving to `std::views::split`. The two disagree on trailing delimiters:
    /// `"a,b,"` yields two elements here and three under `views::split`.
    auto split(const std::string &s, char delim) -> std::vector<std::string>
    {
        std::vector<std::string> elems;
        if (s.empty())
        {
            elems.emplace_back();
        }
        else
        {
            std::stringstream ss;
            ss.str(s);
            std::string item;
            while (std::getline(ss, item, delim))
            {
                elems.push_back(item);
            }
        }
        return elems;
    }

    /// @brief Joins a range into a delimited string.
    ///
    /// A single trailing delimiter is stripped from the result.
    ///
    /// @tparam T Any range whose elements are streamable.
    /// @param v The range to join.
    /// @param delim The separator placed between elements.
    /// @return The joined string.
    template <typename T> auto join(const T &v, std::string_view delim = ",") -> std::string
    {
        std::ostringstream s;
        auto beg = std::begin(v);
        auto end = std::end(v);
        if (beg != end)
        {
            s << *beg++;
        }
        while (beg != end)
        {
            s << delim << *beg++;
        }
        auto rval = s.str();
        if (!rval.empty() && delim.size() == 1 && rval.back() == delim[0])
        {
            rval.pop_back();
        }
        return rval;
    }

    /// @brief Joins a range into a delimited string, transforming each element.
    ///
    /// @tparam T Any range.
    /// @tparam callable_t Any callable mapping an element to something streamable.
    /// @param v The range to join.
    /// @param func Applied to each element before it is written.
    /// @param delim The separator placed between elements.
    /// @return The joined string.
    template <typename T, typename callable_t>
        requires(!std::is_constructible_v<std::string, callable_t>)
    auto join(const T &v, callable_t func, std::string_view delim = ",") -> std::string
    {
        std::ostringstream s;
        auto beg = std::begin(v);
        auto end = std::end(v);
        auto loc = s.tellp();
        while (beg != end)
        {
            auto nloc = s.tellp();
            if (nloc > loc)
            {
                s << delim;
                loc = nloc;
            }
            s << func(*beg++);
        }
        return s.str();
    }

    /// @brief Joins a range into a delimited string in reverse order.
    ///
    /// @tparam T Any bidirectional range whose elements are streamable.
    /// @param v The range to join.
    /// @param delim The separator placed between elements.
    /// @return The joined string.
    template <typename T> auto rjoin(const T &v, std::string_view delim = ",") -> std::string
    {
        std::ostringstream s;
        bool first = true;
        for (const auto &element : v | std::views::reverse)
        {
            if (!first)
            {
                s << delim;
            }
            first = false;
            s << element;
        }
        return s.str();
    }

    /// @brief Removes leading whitespace in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @return A reference to @p str.
    auto ltrim(std::string &str) -> std::string &
    {
        const auto it = std::ranges::find_if(str, [](char ch) { return !std::isspace<char>(ch, std::locale()); });
        str.erase(str.begin(), it);
        return str;
    }

    /// @brief Removes leading characters that appear in @p filter, in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @param[in] filter The set of characters to strip.
    /// @return A reference to @p str.
    constexpr auto ltrim(std::string &str, std::string_view filter) -> std::string &
    {
        const auto it =
            std::ranges::find_if(str, [filter](char ch) { return filter.find(ch) == std::string_view::npos; });
        str.erase(str.begin(), it);
        return str;
    }

    /// @brief Removes trailing whitespace in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @return A reference to @p str.
    auto rtrim(std::string &str) -> std::string &
    {
        const auto it = std::ranges::find_if(str | std::views::reverse,
                                             [](char ch) { return !std::isspace<char>(ch, std::locale()); });
        str.erase(it.base(), str.end());
        return str;
    }

    /// @brief Removes trailing characters that appear in @p filter, in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @param[in] filter The set of characters to strip.
    /// @return A reference to @p str.
    constexpr auto rtrim(std::string &str, std::string_view filter) -> std::string &
    {
        const auto it = std::ranges::find_if(str | std::views::reverse,
                                             [filter](char ch) { return filter.find(ch) == std::string_view::npos; });
        str.erase(it.base(), str.end());
        return str;
    }

    /// @brief Removes whitespace from both ends, in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @return A reference to @p str.
    auto trim(std::string &str) -> std::string &
    {
        return ltrim(rtrim(str));
    }

    /// @brief Removes characters in @p filter from both ends, in place.
    ///
    /// @param[in,out] str The string to trim.
    /// @param[in] filter The set of characters to strip.
    /// @return A reference to @p str.
    constexpr auto trim(std::string &str, std::string_view filter) -> std::string &
    {
        return ltrim(rtrim(str, filter), filter);
    }

    /// @brief Returns a copy with whitespace removed from both ends.
    ///
    /// @param str The string to trim.
    /// @return The trimmed copy.
    auto trim_copy(const std::string &str) -> std::string
    {
        std::string s = str;
        return trim(s);
    }

    /// @brief Returns a copy with characters in @p filter removed from both ends.
    ///
    /// @param str The string to trim.
    /// @param filter The set of characters to strip.
    /// @return The trimmed copy.
    constexpr auto trim_copy(const std::string &str, std::string_view filter) -> std::string
    {
        std::string s = str;
        return trim(s, filter);
    }

    /// @brief Strips one matched pair of surrounding quotes, in place.
    ///
    /// Recognises `"`, `'` and `` ` ``.
    ///
    /// @param[in,out] str The string to unquote.
    /// @return A reference to @p str.
    constexpr auto remove_quotes(std::string &str) -> std::string &
    {
        if (str.length() > 1 && (str.front() == '"' || str.front() == '\'' || str.front() == '`'))
        {
            if (str.front() == str.back())
            {
                str.pop_back();
                str.erase(str.begin(), str.begin() + 1);
            }
        }
        return str;
    }

    /// @brief Inserts @p leader after every newline.
    ///
    /// @param leader The text to insert after each line break.
    /// @param input The text to reflow.
    /// @return The reflowed text.
    auto fix_newlines(const std::string &leader, std::string input) -> std::string
    {
        std::string::size_type n = 0;
        while (n != std::string::npos && n < input.size())
        {
            n = input.find_first_of("\r\n", n);
            if (n != std::string::npos)
            {
                input = input.substr(0, n + 1) + leader + input.substr(n + 1);
                n += leader.size();
            }
        }
        return input;
    }

    /// @brief Writes an indented, comma-separated alias list.
    ///
    /// Writes nothing when @p aliases is empty.
    ///
    /// @param out The stream to write to.
    /// @param aliases The aliases to list.
    /// @param wid Field width for the leading label.
    /// @return @p out, for chaining.
    auto format_aliases(std::ostream &out, const std::vector<std::string> &aliases, std::size_t wid) -> std::ostream &
    {
        if (!aliases.empty())
        {
            out << std::setw(static_cast<int>(wid)) << "     aliases: ";
            bool front = true;
            for (const auto &alias : aliases)
            {
                if (!front)
                {
                    out << ", ";
                }
                else
                {
                    front = false;
                }
                out << fix_newlines("              ", alias);
            }
            out << "\n";
        }
        return out;
    }

    /// @brief Tests whether a character may begin an option name.
    ///
    /// @tparam T A character type.
    /// @param c The character to test.
    /// @return `true` if @p c is a valid leading character.
    template <typename T> constexpr auto valid_first_char(T c) -> bool
    {
        return ((c != '-') && (static_cast<unsigned char>(c) > 33));
    }

    /// @brief Tests whether a character may appear after the first in an option name.
    ///
    /// @tparam T A character type.
    /// @param c The character to test.
    /// @return `true` if @p c is a valid subsequent character.
    template <typename T> constexpr auto valid_later_char(T c) -> bool
    {
        return ((c != '=') && (c != ':') && (c != '{') && ((static_cast<unsigned char>(c) > 32) || c == '\t'));
    }

    /// @brief Tests whether a string is a usable option or subcommand name.
    ///
    /// @param str The candidate name.
    /// @return `true` if every character is permitted in its position.
    constexpr auto valid_name_string(const std::string &str) -> bool
    {
        if (str.empty() || !valid_first_char(str[0]))
        {
            return false;
        }
        return std::all_of(str.begin() + 1, str.end(), [](char c) { return valid_later_char(c); });
    }

    /// @brief Tests whether a string is usable as an alias.
    ///
    /// Aliases are unrestricted apart from newlines and embedded nulls.
    ///
    /// @param str The candidate alias.
    /// @return `true` if @p str contains neither a newline nor a null.
    constexpr auto valid_alias_name_string(const std::string &str) -> bool
    {
        return ((str.find_first_of('\n') == std::string::npos) && (str.find_first_of('\0') == std::string::npos));
    }

    /// @brief Tests whether an argument is a group separator.
    ///
    /// @param str The argument to test.
    /// @return `true` for an empty string or `"%%"`.
    constexpr auto is_separator(const std::string &str) -> bool
    {
        return (str.empty() || (str.size() == 2 && str[0] == '%' && str[1] == '%'));
    }

    /// @brief Tests whether every character is alphabetic in the current locale.
    ///
    /// @param str The string to test.
    /// @return `true` if @p str is entirely alphabetic.
    auto isalpha(const std::string &str) -> bool
    {
        return std::ranges::all_of(str, [](char c) { return std::isalpha(c, std::locale()); });
    }

    /// @brief Returns a lowercased copy, using the current locale.
    ///
    /// @param str The string to convert.
    /// @return The lowercased string.
    auto to_lower(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(), [](char x) { return std::tolower(x, std::locale()); });
        return str;
    }

    /// @brief Returns a copy with every underscore removed.
    ///
    /// @param str The string to strip.
    /// @return The stripped string.
    constexpr auto remove_underscore(std::string str) -> std::string
    {
        std::erase(str, '_');
        return str;
    }

    /// @brief Returns the characters treated as digit-group separators.
    ///
    /// Always includes `_` and `'`, plus the locale's thousands separator.
    ///
    /// @return The separator characters.
    auto get_group_separators() -> std::string
    {
        std::string separators {"_'"};
        const char group_separator = std::use_facet<std::numpunct<char>>(std::locale()).thousands_sep();
        separators.push_back(group_separator);
        return separators;
    }

    /// @brief Replaces every occurrence of @p from with @p to.
    ///
    /// @param str The string to rewrite.
    /// @param from The text to search for.
    /// @param to The replacement text.
    /// @return The rewritten string.
    constexpr auto find_and_replace(std::string str, std::string_view from, std::string_view to) -> std::string
    {
        std::size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }

    /// @brief Tests whether a flag specification carries default values.
    ///
    /// @param flags The flag specification.
    /// @return `true` if @p flags contains `{` or `!`.
    constexpr auto has_default_flag_values(const std::string &flags) -> bool
    {
        return (flags.find_first_of("{!") != std::string::npos);
    }

    /// @brief Strips `{...}` default values and `!` negation markers, in place.
    ///
    /// @param[in,out] flags The flag specification to rewrite.
    constexpr auto remove_default_flag_values(std::string &flags) -> void
    {
        auto loc = flags.find_first_of('{', 2);
        while (loc != std::string::npos)
        {
            const auto finish = flags.find_first_of("},", loc + 1);
            if ((finish != std::string::npos) && (flags[finish] == '}'))
            {
                flags.erase(flags.begin() + static_cast<std::ptrdiff_t>(loc),
                            flags.begin() + static_cast<std::ptrdiff_t>(finish) + 1);
            }
            loc = flags.find_first_of('{', loc + 1);
        }
        std::erase(flags, '!');
    }

    /// @brief Finds a name in a list, optionally ignoring case and underscores.
    ///
    /// @param name The name to search for.
    /// @param names The list to search.
    /// @param ignore_case Compare case-insensitively.
    /// @param ignore_underscore Ignore underscores on both sides of the comparison.
    /// @return The index of the match, or `std::nullopt` if there is none.
    auto find_member(std::string name,
                     const std::vector<std::string> &names,
                     bool ignore_case = false,
                     bool ignore_underscore = false) -> std::optional<std::size_t>
    {
        auto it = std::end(names);
        if (ignore_case)
        {
            if (ignore_underscore)
            {
                name = to_lower(remove_underscore(name));
                it = std::ranges::find_if(
                    names, [&name](const std::string &local) { return to_lower(remove_underscore(local)) == name; });
            }
            else
            {
                name = to_lower(name);
                it = std::ranges::find_if(names, [&name](const std::string &local) { return to_lower(local) == name; });
            }
        }
        else if (ignore_underscore)
        {
            name = remove_underscore(name);
            it = std::ranges::find_if(names,
                                      [&name](const std::string &local) { return remove_underscore(local) == name; });
        }
        else
        {
            it = std::ranges::find(names, name);
        }

        if (it == std::end(names))
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(std::begin(names), it));
    }

    /// @brief Repeatedly applies @p modify at each occurrence of @p trigger.
    ///
    /// @tparam callable_t Callable taking the string and a position, returning the
    /// position to resume searching from.
    /// @param str The string to rewrite.
    /// @param trigger The text to search for.
    /// @param modify Applied at each occurrence.
    /// @return The rewritten string.
    template <typename callable_t>
    auto find_and_modify(std::string str, std::string_view trigger, callable_t modify) -> std::string
    {
        std::size_t start_pos = 0;
        while ((start_pos = str.find(trigger, start_pos)) != std::string::npos)
        {
            start_pos = modify(str, start_pos);
        }
        return str;
    }

    /// @brief Tests whether a string contains a character needing an escape.
    ///
    /// @param str The string to test.
    /// @return `true` if any character appears in @ref escaped_chars.
    constexpr auto has_escapable_character(const std::string &str) -> bool
    {
        return (str.find_first_of(escaped_chars) != std::string::npos);
    }

    /// @brief Returns a copy with backslash escapes applied.
    ///
    /// @param str The string to escape.
    /// @return The escaped string.
    constexpr auto add_escaped_characters(const std::string &str) -> std::string
    {
        std::string out;
        out.reserve(str.size() + 4);
        for (char s : str)
        {
            const auto sloc = escaped_chars.find_first_of(s);
            if (sloc != std::string_view::npos)
            {
                out.push_back('\\');
                out.push_back(escaped_chars_code[sloc]);
            }
            else
            {
                out.push_back(s);
            }
        }
        return out;
    }

    /// @brief Returns a copy with backslash escapes resolved.
    ///
    /// Understands the codes in @ref escaped_chars_code, `\\0`, and the `\\uXXXX`
    /// and `\\UXXXXXXXX` Unicode forms.
    ///
    /// @param str The string to unescape.
    /// @return The unescaped string.
    /// @throws std::invalid_argument If an escape sequence is truncated or unknown.
    constexpr auto remove_escaped_characters(const std::string &str) -> std::string
    {
        std::string out;
        out.reserve(str.size());
        for (auto loc = str.begin(); loc < str.end(); ++loc)
        {
            if (*loc == '\\')
            {
                if (str.end() - loc < 2)
                {
                    throw std::invalid_argument("invalid escape sequence " + str);
                }
                const auto ecloc = escaped_chars_code.find_first_of(*(loc + 1));
                if (ecloc != std::string_view::npos)
                {
                    out.push_back(escaped_chars[ecloc]);
                    ++loc;
                }
                else if (*(loc + 1) == 'u')
                {
                    if (str.end() - loc < 6)
                    {
                        throw std::invalid_argument("unicode sequence must have 4 hex codes " + str);
                    }
                    std::uint32_t code {0};
                    std::uint32_t mplier {16 * 16 * 16};
                    for (int ii = 2; ii < 6; ++ii)
                    {
                        const std::uint32_t res = hex_convert(*(loc + ii));
                        if (res > 0x0F)
                        {
                            throw std::invalid_argument("unicode sequence must have 4 hex codes " + str);
                        }
                        code += res * mplier;
                        mplier = mplier / 16;
                    }
                    append_codepoint(out, code);
                    loc += 5;
                }
                else if (*(loc + 1) == 'U')
                {
                    if (str.end() - loc < 10)
                    {
                        throw std::invalid_argument("unicode sequence must have 8 hex codes " + str);
                    }
                    std::uint32_t code {0};
                    std::uint32_t mplier {16 * 16 * 16 * 16 * 16 * 16 * 16};
                    for (int ii = 2; ii < 10; ++ii)
                    {
                        const std::uint32_t res = hex_convert(*(loc + ii));
                        if (res > 0x0F)
                        {
                            throw std::invalid_argument("unicode sequence must have 8 hex codes " + str);
                        }
                        code += res * mplier;
                        mplier = mplier / 16;
                    }
                    append_codepoint(out, code);
                    loc += 9;
                }
                else if (*(loc + 1) == '0')
                {
                    out.push_back('\0');
                    ++loc;
                }
                else
                {
                    throw std::invalid_argument(std::string("unrecognized escape sequence \\") + *(loc + 1) + " in " +
                                                str);
                }
            }
            else
            {
                out.push_back(*loc);
            }
        }
        return out;
    }

    /// @brief Strips surrounding quotes from every argument, in place.
    ///
    /// Double-quoted arguments additionally have their escapes resolved.
    ///
    /// @param[in,out] args The arguments to unquote.
    constexpr auto remove_quotes(std::vector<std::string> &args) -> void
    {
        for (auto &arg : args)
        {
            if (arg.front() == '\"' && arg.back() == '\"')
            {
                remove_quotes(arg);
                arg = remove_escaped_characters(arg);
            }
            else
            {
                remove_quotes(arg);
            }
        }
    }

    /// @brief Finds the index closing a quoted or bracketed sequence.
    ///
    /// Nested brackets and quotes are tracked, so the returned index matches the
    /// opener at @p start rather than the first candidate encountered.
    ///
    /// @param str The text to scan.
    /// @param start Index of the opening character.
    /// @param closure_char The character that closes the sequence.
    /// @return Index of the closing character, or `str.size()` if unterminated.
    constexpr auto close_sequence(const std::string &str, std::size_t start, char closure_char) -> std::size_t
    {
        auto bracket_loc = match_bracket_chars.find(closure_char);
        switch (bracket_loc)
        {
        case 0:
            return close_string_quote(str, start, closure_char);
        case 1:
        case 2:
        case std::string_view::npos:
            return close_literal_quote(str, start, closure_char);
        default:
            break;
        }

        std::string closures(1, closure_char);
        auto loc = start + 1;

        while (loc < str.size())
        {
            if (str[loc] == closures.back())
            {
                closures.pop_back();
                if (closures.empty())
                {
                    return loc;
                }
            }
            bracket_loc = bracket_chars.find(str[loc]);
            if (bracket_loc != std::string_view::npos)
            {
                switch (bracket_loc)
                {
                case 0:
                    loc = close_string_quote(str, loc, str[loc]);
                    break;
                case 1:
                case 2:
                    loc = close_literal_quote(str, loc, str[loc]);
                    break;
                default:
                    closures.push_back(match_bracket_chars[bracket_loc]);
                    break;
                }
            }
            ++loc;
        }
        if (loc > str.size())
        {
            loc = str.size();
        }
        return loc;
    }

    /// @brief Splits a command line into arguments, respecting quotes and brackets.
    ///
    /// @param str The text to split.
    /// @param delimiter Separator to split on; `'\0'` splits on whitespace.
    /// @return The extracted arguments.
    auto split_up(std::string str, char delimiter = '\0') -> std::vector<std::string>
    {
        const auto find_ws = [delimiter](char ch) {
            return (delimiter == '\0') ? std::isspace<char>(ch, std::locale()) : (ch == delimiter);
        };
        trim(str);

        std::vector<std::string> output;
        while (!str.empty())
        {
            if (bracket_chars.find_first_of(str[0]) != std::string_view::npos)
            {
                const auto bracket_loc = bracket_chars.find_first_of(str[0]);
                const auto end = close_sequence(str, 0, match_bracket_chars[bracket_loc]);
                if (end >= str.size())
                {
                    output.push_back(std::move(str));
                    str.clear();
                }
                else
                {
                    output.push_back(str.substr(0, end + 1));
                    if (end + 2 < str.size())
                    {
                        str = str.substr(end + 2);
                    }
                    else
                    {
                        str.clear();
                    }
                }
            }
            else
            {
                const auto it = std::ranges::find_if(str, find_ws);
                if (it != std::end(str))
                {
                    output.emplace_back(str.begin(), it);
                    str = std::string(it + 1, str.end());
                }
                else
                {
                    output.push_back(str);
                    str.clear();
                }
            }
            trim(str);
        }
        return output;
    }

    /// @brief Neutralises a separator that begins a quoted value.
    ///
    /// @param[in,out] str The text being scanned.
    /// @param[in] offset Index of the candidate separator.
    /// @return The index to resume scanning from.
    constexpr auto escape_detect(std::string &str, std::size_t offset) -> std::size_t
    {
        const auto next = str[offset + 1];
        if ((next == '\"') || (next == '\'') || (next == '`'))
        {
            const auto astart = str.find_last_of("-/ \"'`", offset - 1);
            if (astart != std::string::npos)
            {
                if (str[astart] == ((str[offset] == '=') ? '-' : '/'))
                {
                    str[offset] = ' ';
                }
            }
        }
        return offset + 1;
    }

    /// @brief Encodes non-printable characters as a binary-escaped string.
    ///
    /// When anything is escaped, or when @p force is set, the result is wrapped
    /// as `'B"(...)"'`.
    ///
    /// @param string_to_escape The string to encode.
    /// @param force Wrap the result even when nothing needed escaping.
    /// @return The encoded string.
    auto binary_escape_string(const std::string &string_to_escape, bool force = false) -> std::string
    {
        std::string escaped_string {};
        for (char c : string_to_escape)
        {
            if (std::isprint(static_cast<unsigned char>(c)) == 0)
            {
                escaped_string += std::format("\\x{:02x}", static_cast<unsigned char>(c));
            }
            else if (c == 'x' || c == 'X')
            {
                if (!escaped_string.empty() && escaped_string.back() == '\\')
                {
                    escaped_string += (c == 'x') ? "\\x78" : "\\x58";
                }
                else
                {
                    escaped_string.push_back(c);
                }
            }
            else
            {
                escaped_string.push_back(c);
            }
        }
        if (escaped_string != string_to_escape || force)
        {
            auto sq_loc = escaped_string.find('\'');
            while (sq_loc != std::string::npos)
            {
                escaped_string[sq_loc] = '\\';
                escaped_string.insert(sq_loc + 1, "x27");
                sq_loc = escaped_string.find('\'');
            }
            escaped_string.insert(0, "'B\"(");
            escaped_string.push_back(')');
            escaped_string.push_back('"');
            escaped_string.push_back('\'');
        }
        return escaped_string;
    }

    /// @brief Tests whether a string carries the binary-escape wrapper.
    ///
    /// @param escaped_string The string to test.
    /// @return `true` if @p escaped_string is wrapped as `B"(...)"` or `'B"(...)"'`.
    constexpr auto is_binary_escaped_string(const std::string &escaped_string) -> bool
    {
        const std::size_t ssize = escaped_string.size();
        if (escaped_string.compare(0, 3, "B\"(") == 0 && escaped_string.compare(ssize - 2, 2, ")\"") == 0)
        {
            return true;
        }
        return (escaped_string.compare(0, 4, "'B\"(") == 0 && escaped_string.compare(ssize - 3, 3, ")\"'") == 0);
    }

    /// @brief Decodes a binary-escaped string.
    ///
    /// @param escaped_string The string to decode.
    /// @return The decoded string, or @p escaped_string unchanged if it is not wrapped.
    constexpr auto extract_binary_string(const std::string &escaped_string) -> std::string
    {
        std::size_t start {0};
        std::size_t tail {0};
        const std::size_t ssize = escaped_string.size();
        if (escaped_string.compare(0, 3, "B\"(") == 0 && escaped_string.compare(ssize - 2, 2, ")\"") == 0)
        {
            start = 3;
            tail = 2;
        }
        else if (escaped_string.compare(0, 4, "'B\"(") == 0 && escaped_string.compare(ssize - 3, 3, ")\"'") == 0)
        {
            start = 4;
            tail = 3;
        }
        if (start == 0)
        {
            return escaped_string;
        }
        std::string outstring;
        outstring.reserve(ssize - start - tail);
        std::size_t loc = start;
        while (loc < ssize - tail)
        {
            if (escaped_string[loc] == '\\' && (escaped_string[loc + 1] == 'x' || escaped_string[loc + 1] == 'X'))
            {
                const std::uint32_t res1 = hex_convert(escaped_string[loc + 2]);
                const std::uint32_t res2 = hex_convert(escaped_string[loc + 3]);
                if (res1 <= 0x0F && res2 <= 0x0F)
                {
                    loc += 4;
                    outstring.push_back(static_cast<char>(res1 * 16 + res2));
                    continue;
                }
            }
            outstring.push_back(escaped_string[loc]);
            ++loc;
        }
        return outstring;
    }

    /// @brief Unwraps a quoted, literal, or binary-escaped string in place.
    ///
    /// @param[in,out] str The string to unwrap.
    /// @param[in] string_char The character delimiting escaped strings.
    /// @param[in] literal_char The character delimiting literal strings.
    /// @param[in] disable_secondary_array_processing Skip the nested-array rewrite.
    /// @return `true` if @p str was wrapped and has been unwrapped.
    constexpr auto process_quoted_string(std::string &str,
                                         char string_char = '\"',
                                         char literal_char = '\'',
                                         bool disable_secondary_array_processing = false) -> bool
    {
        if (str.size() <= 1)
        {
            return false;
        }
        if (is_binary_escaped_string(str))
        {
            str = extract_binary_string(str);
            if (!disable_secondary_array_processing)
            {
                handle_secondary_array(str);
            }
            return true;
        }
        if (str.front() == string_char && str.back() == string_char)
        {
            remove_outer(str, string_char);
            if (str.find_first_of('\\') != std::string::npos)
            {
                str = remove_escaped_characters(str);
            }
            if (!disable_secondary_array_processing)
            {
                handle_secondary_array(str);
            }
            return true;
        }
        if ((str.front() == literal_char || str.front() == '`') && str.back() == str.front())
        {
            remove_outer(str, str.front());
            if (!disable_secondary_array_processing)
            {
                handle_secondary_array(str);
            }
            return true;
        }
        return false;
    }

    /// @brief Reads an environment variable.
    ///
    /// @param env_name The variable to read.
    /// @return The variable's value, or an empty string if it is not set.
    auto get_environment_value(const std::string &env_name) -> std::string
    {
        std::string ename_string;
#ifdef _MSC_VER
        char *raw = nullptr;
        std::size_t sz = 0;
        if (_dupenv_s(&raw, &sz, env_name.c_str()) == 0 && raw != nullptr)
        {
            const std::unique_ptr<char, decltype(&std::free)> buffer {raw, &std::free};
            ename_string = std::string(buffer.get());
        }
#else
        const char *buffer = std::getenv(env_name.c_str());
        if (buffer != nullptr)
        {
            ename_string = std::string(buffer);
        }
#endif
        return ename_string;
    }

    /// @brief Writes text word-wrapped to a fixed width, prefixing each line.
    ///
    /// Existing line breaks in @p text are preserved; each resulting line is then
    /// wrapped independently.
    ///
    /// @param out The stream to write to.
    /// @param text The text to wrap.
    /// @param paragraph_width Maximum characters per line, excluding the prefix.
    /// @param line_prefix Written at the start of each line.
    /// @param skip_prefix_on_first_line Omit the prefix on the first line.
    /// @return @p out, for chaining.
    auto stream_out_as_paragraph(std::ostream &out,
                                 const std::string &text,
                                 std::size_t paragraph_width,
                                 const std::string &line_prefix = "",
                                 bool skip_prefix_on_first_line = false) -> std::ostream &
    {
        if (!skip_prefix_on_first_line)
        {
            out << line_prefix;
        }

        std::istringstream lss(text);
        std::string line;
        while (std::getline(lss, line))
        {
            std::istringstream iss(line);
            std::string word;
            std::size_t chars_written = 0;

            while (iss >> word)
            {
                if (chars_written > 0 && (word.length() + 1 + chars_written > paragraph_width))
                {
                    out << '\n' << line_prefix;
                    chars_written = 0;
                }
                if (chars_written == 0)
                {
                    out << word;
                    chars_written += word.length();
                }
                else
                {
                    out << ' ' << word;
                    chars_written += word.length() + 1;
                }
            }

            if (!lss.eof())
            {
                out << '\n' << line_prefix;
            }
        }
        return out;
    }

} // namespace cli::detail
