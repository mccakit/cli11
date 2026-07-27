/// @file
/// @brief Reading and writing configuration files.
///
/// Implements @ref cli::config_base_t, declared in the `config_fwd` partition.
/// Writing walks an @ref cli::app_t and emits one entry per configurable option,
/// quoting values as needed; reading parses entries back into
/// @ref cli::config_item_t values that `app_t` then applies as if they had been
/// given on the command line.
///
/// The format is TOML-shaped but every piece of punctuation is configurable, so
/// the same code handles INI. Section nesting maps to subcommands, with
/// `[a.b]` addressing subcommand `b` of subcommand `a`.

export module cli11:config;

import std;
import :string_tools;
import :error;
import :option;
import :config_fwd;
import :type_tools;
import :app;

export namespace cli
{

    namespace detail
    {

        /// @brief Quotes and escapes one value for writing to a configuration file.
        ///
        /// @param arg The value to convert.
        /// @param string_quote The character used around escaped strings.
        /// @param literal_quote The character used around literal strings.
        /// @param disable_multi_line Never use the triple-quoted multi-line form.
        /// @return The value, quoted if it needs to be.
        auto convert_arg_for_ini(const std::string &arg,
                                 char string_quote = '"',
                                 char literal_quote = '\'',
                                 bool disable_multi_line = false) -> std::string;

        /// @brief Joins values into one configuration entry, quoting as needed.
        ///
        /// @param args The values to join.
        /// @param sep_char The character placed between values.
        /// @param array_start The character opening an array; `'\0'` disables arrays.
        /// @param array_end The character closing an array; `'\0'` disables arrays.
        /// @param string_quote The character used around escaped strings.
        /// @param literal_quote The character used around literal strings.
        /// @return The joined entry.
        auto ini_join(const std::vector<std::string> &args,
                      char sep_char = ',',
                      char array_start = '[',
                      char array_end = ']',
                      char string_quote = '"',
                      char literal_quote = '\'') -> std::string;

        /// @brief Splits a section path and a name into a list of parent sections.
        ///
        /// @param[in] section The section the entry was found in.
        /// @param[in,out] name The entry name; reduced to its last component.
        /// @param[in] parent_separator The character separating nested names.
        /// @return The parent sections, outermost first.
        auto generate_parents(const std::string &section, std::string &name, char parent_separator)
            -> std::vector<std::string>;

        /// @brief Emits the section transitions needed before a new section.
        ///
        /// Compares the section being opened against the one already open and appends
        /// the closing and opening markers required to get from one to the other.
        ///
        /// @param[in,out] output The entries built so far.
        /// @param[in] current_section The section being opened.
        /// @param[in] parent_separator The character separating nested names.
        auto check_parent_segments(std::vector<config_item_t> &output,
                                   const std::string &current_section,
                                   char parent_separator) -> void;

    } // namespace detail

    // =============================================
    // Implementation
    // =============================================

    /// @brief Opening and closing delimiter for a multi-line literal string.
    constexpr std::string_view multiline_literal_quote = R"(''')";

    /// @brief Opening and closing delimiter for a multi-line escaped string.
    constexpr std::string_view multiline_string_quote = R"(""")";

    namespace detail
    {

        /// @brief Reports whether a string is safe to write unescaped.
        ///
        /// Newlines and tabs count as printable, since the multi-line form can carry them.
        ///
        /// @param test_string The string to test.
        /// @return `true` if every character is printable.
        auto is_printable(const std::string &test_string) -> bool
        {
            return std::ranges::all_of(test_string, [](char x) {
                return (std::isprint(static_cast<unsigned char>(x)) != 0 || x == '\n' || x == '\t');
            });
        }

        auto convert_arg_for_ini(const std::string &arg, char string_quote, char literal_quote, bool disable_multi_line)
            -> std::string
        {
            if (arg.empty())
            {
                return std::string(2, string_quote);
            }

            // Literals the format understands without quoting.
            if (arg == "true" || arg == "false" || arg == "nan" || arg == "inf")
            {
                return arg;
            }

            // Numbers are written bare, but hex is excluded here because the
            // floating-point parser accepts some hex forms and would round-trip them
            // into something else.
            if (arg.compare(0, 2, "0x") != 0 && arg.compare(0, 2, "0X") != 0)
            {
                using detail::lexical_cast;
                double val = 0.0;
                if (lexical_cast(arg, val))
                {
                    if (arg.find_first_not_of("0123456789.-+eE") == std::string::npos)
                    {
                        return arg;
                    }
                }
            }

            // A lone character needs quoting, and a lone apostrophe cannot use the
            // literal form.
            if (arg.size() == 1)
            {
                if (std::isprint(static_cast<unsigned char>(arg.front())) == 0)
                {
                    return binary_escape_string(arg);
                }
                if (arg == "'")
                {
                    return std::string(1, string_quote) + "'" + string_quote;
                }
                return std::string(1, literal_quote) + arg + literal_quote;
            }

            // Hex, octal, and binary literals are written bare when well formed.
            if (arg.front() == '0')
            {
                if (arg[1] == 'x')
                {
                    if (std::ranges::all_of(arg.begin() + 2, arg.end(), [](char x) {
                            return (x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f');
                        }))
                    {
                        return arg;
                    }
                }
                else if (arg[1] == 'o')
                {
                    if (std::ranges::all_of(arg.begin() + 2, arg.end(), [](char x) { return (x >= '0' && x <= '7'); }))
                    {
                        return arg;
                    }
                }
                else if (arg[1] == 'b')
                {
                    if (std::ranges::all_of(arg.begin() + 2, arg.end(), [](char x) { return (x == '0' || x == '1'); }))
                    {
                        return arg;
                    }
                }
            }

            if (!is_printable(arg))
            {
                return binary_escape_string(arg);
            }

            if (detail::has_escapable_character(arg))
            {
                // Long values with escapes are more readable as multi-line literals,
                // unless they contain the delimiter itself.
                if (arg.size() > 100 && !disable_multi_line)
                {
                    if (arg.find(multiline_literal_quote) != std::string::npos)
                    {
                        return binary_escape_string(arg, true);
                    }
                    std::string return_string {multiline_literal_quote};
                    return_string.reserve(7 + arg.size());
                    if (arg.front() == '\n' || arg.front() == '\r')
                    {
                        return_string.push_back('\n');
                    }
                    return_string.append(arg);
                    if (arg.back() == '\n' || arg.back() == '\r')
                    {
                        return_string.push_back('\n');
                    }
                    return_string.append(multiline_literal_quote);
                    return return_string;
                }
                return std::string(1, string_quote) + detail::add_escaped_characters(arg) + string_quote;
            }

            return std::string(1, string_quote) + arg + string_quote;
        }

        auto ini_join(const std::vector<std::string> &args,
                      char sep_char,
                      char array_start,
                      char array_end,
                      char string_quote,
                      char literal_quote) -> std::string
        {
            bool disable_multi_line {false};
            std::string joined;

            if (args.size() > 1 && array_start != '\0')
            {
                joined.push_back(array_start);
                disable_multi_line = true;
            }

            std::size_t start = 0;
            for (const auto &arg : args)
            {
                if (start++ > 0)
                {
                    joined.push_back(sep_char);
                    if (!std::isspace<char>(sep_char, std::locale()))
                    {
                        joined.push_back(' ');
                    }
                }
                joined.append(convert_arg_for_ini(arg, string_quote, literal_quote, disable_multi_line));
            }

            if (args.size() > 1 && array_end != '\0')
            {
                joined.push_back(array_end);
            }
            return joined;
        }

        auto generate_parents(const std::string &section, std::string &name, char parent_separator)
            -> std::vector<std::string>
        {
            std::vector<std::string> parents;

            // The "default" section is the root, so it contributes no parent.
            if (detail::to_lower(section) != "default")
            {
                if (section.find(parent_separator) != std::string::npos)
                {
                    parents = detail::split_up(section, parent_separator);
                }
                else
                {
                    parents = {section};
                }
            }

            // A dotted name carries its own parents; the last component is the name.
            if (name.find(parent_separator) != std::string::npos)
            {
                std::vector<std::string> plist = detail::split_up(name, parent_separator);
                name = plist.back();
                plist.pop_back();
                parents.insert(parents.end(), plist.begin(), plist.end());
            }

            try
            {
                detail::remove_quotes(parents);
            }
            catch (const std::invalid_argument &iarg)
            {
                throw parse_error_t(iarg.what(), exit_codes_t::invalid_error);
            }
            return parents;
        }

        auto check_parent_segments(std::vector<config_item_t> &output,
                                   const std::string &current_section,
                                   char parent_separator) -> void
        {
            std::string estring;
            auto parents = detail::generate_parents(current_section, estring, parent_separator);

            if (!output.empty() && output.back().name == "--")
            {
                // The previous section is still open. Close it down to the depth the
                // new section shares with it.
                const std::size_t msize = (parents.size() > 1U) ? parents.size() : 2;
                while (output.back().parents.size() >= msize)
                {
                    output.push_back(output.back());
                    output.back().parents.pop_back();
                }

                if (parents.size() > 1)
                {
                    std::size_t common = 0;
                    const std::size_t mpair = (std::min)(output.back().parents.size(), parents.size() - 1);
                    for (std::size_t ii = 0; ii < mpair; ++ii)
                    {
                        if (output.back().parents[ii] != parents[ii])
                        {
                            break;
                        }
                        ++common;
                    }

                    if (common == mpair)
                    {
                        output.pop_back();
                    }
                    else
                    {
                        while (output.back().parents.size() > common + 1)
                        {
                            output.push_back(output.back());
                            output.back().parents.pop_back();
                        }
                    }

                    for (std::size_t ii = common; ii < parents.size() - 1; ++ii)
                    {
                        output.emplace_back();
                        output.back().parents.assign(parents.begin(),
                                                     parents.begin() + static_cast<std::ptrdiff_t>(ii) + 1);
                        output.back().name = "++";
                    }
                }
            }
            else if (parents.size() > 1)
            {
                for (std::size_t ii = 0; ii < parents.size() - 1; ++ii)
                {
                    output.emplace_back();
                    output.back().parents.assign(parents.begin(),
                                                 parents.begin() + static_cast<std::ptrdiff_t>(ii) + 1);
                    output.back().name = "++";
                }
            }

            // An entry named "++" with no inputs marks a section opening.
            output.emplace_back();
            output.back().parents = std::move(parents);
            output.back().name = "++";
        }

        /// @brief Reports whether a line ends with a triple-quote delimiter.
        ///
        /// @param full_string The line to inspect.
        /// @param check The quote character to look for.
        /// @return `true` if the line ends with three of @p check.
        auto has_ml_string(const std::string &full_string, char check) -> bool
        {
            if (full_string.length() < 3)
            {
                return false;
            }
            auto it = full_string.rbegin();
            return (*it == check) && (*(it + 1) == check) && (*(it + 2) == check);
        }

        /// @brief Finds an entry with a given section path and name.
        ///
        /// Searches backwards, since a repeated field is normally adjacent to its
        /// earlier occurrence.
        ///
        /// @param items The entries to search.
        /// @param parents The section path to match.
        /// @param name The entry name to match.
        /// @param full_search Search the whole list rather than only the last entry.
        /// @return An iterator to the match, or `items.end()`.
        auto find_matching_config(std::vector<config_item_t> &items,
                                  const std::vector<std::string> &parents,
                                  const std::string &name,
                                  bool full_search) -> std::vector<config_item_t>::iterator
        {
            if (items.empty())
            {
                return items.end();
            }
            auto search = items.end() - 1;
            do
            {
                if (search->parents == parents && search->name == name)
                {
                    return search;
                }
                if (search == items.begin())
                {
                    break;
                }
                --search;
            } while (full_search);
            return items.end();
        }

    } // namespace detail

    auto config_base_t::from_config(std::istream &input) const -> std::vector<config_item_t>
    {
        std::string line;
        std::string buffer;
        std::string current_section = "default";
        std::string previous_section = "default";
        std::vector<config_item_t> output;

        const bool is_default_array = (array_start_ == '[' && array_end_ == ']' && array_separator_ == ',');
        const bool is_ini_array = (array_start_ == '\0' || array_start_ == ' ') && array_start_ == array_end_;
        bool in_section {false};
        bool in_mline_comment {false};
        bool in_mline_value {false};

        // An INI-style reader has no array punctuation of its own, so borrow the TOML
        // characters for parsing values that happen to be bracketed.
        const char a_start = (is_ini_array) ? '[' : array_start_;
        const char a_end = (is_ini_array) ? ']' : array_end_;
        const char a_sep = (is_ini_array && array_separator_ == ' ') ? ',' : array_separator_;
        int current_section_index {0};

        const std::string line_sep_chars {parent_separator_char_, comment_char_, value_delimiter_};

        while (std::getline(input, buffer))
        {
            std::vector<std::string> items_buffer;
            std::string name;
            line = detail::trim_copy(buffer);
            const std::size_t len = line.length();

            // Nothing shorter than three characters can carry meaning.
            if (len < 3)
            {
                continue;
            }

            // A line opening with a triple quote is a multi-line comment; skip to its close.
            if (line.compare(0, 3, multiline_string_quote) == 0 || line.compare(0, 3, multiline_literal_quote) == 0)
            {
                in_mline_comment = true;
                const auto cchar = line.front();
                while (in_mline_comment)
                {
                    if (std::getline(input, line))
                    {
                        detail::trim(line);
                    }
                    else
                    {
                        break;
                    }
                    if (detail::has_ml_string(line, cchar))
                    {
                        in_mline_comment = false;
                    }
                }
                continue;
            }

            if (line.front() == '[' && line.back() == ']')
            {
                if (current_section != "default")
                {
                    // An entry named "--" with no inputs marks a section closing.
                    output.emplace_back();
                    output.back().parents = detail::generate_parents(current_section, name, parent_separator_char_);
                    output.back().name = "--";
                }

                current_section = line.substr(1, len - 2);

                // TOML writes an array of tables as [[name]].
                if (current_section.size() > 1 && current_section.front() == '[' && current_section.back() == ']')
                {
                    current_section = current_section.substr(1, current_section.size() - 2);
                }

                if (detail::to_lower(current_section) == "default")
                {
                    current_section = "default";
                }
                else
                {
                    detail::check_parent_segments(output, current_section, parent_separator_char_);
                }

                in_section = false;
                if (current_section == previous_section)
                {
                    ++current_section_index;
                }
                else
                {
                    current_section_index = 0;
                    previous_section = current_section;
                }
                continue;
            }

            if (line.front() == ';' || line.front() == '#' || line.front() == comment_char_)
            {
                continue;
            }

            // Walk past any quoted run so that a delimiter or comment character inside
            // a quoted value is not mistaken for the real one.
            std::size_t search_start = 0;
            if (line.find_first_of("\"'`") != std::string::npos)
            {
                while (search_start < line.size())
                {
                    const auto test_char = line[search_start];
                    if (test_char == '\"' || test_char == '\'' || test_char == '`')
                    {
                        search_start = detail::close_sequence(line, search_start, line[search_start]);
                        ++search_start;
                    }
                    else if (test_char == value_delimiter_ || test_char == comment_char_)
                    {
                        --search_start;
                        break;
                    }
                    else if (test_char == ' ' || test_char == '\t' || test_char == parent_separator_char_)
                    {
                        ++search_start;
                    }
                    else
                    {
                        search_start = line.find_first_of(line_sep_chars, search_start);
                    }
                }
            }

            auto delimiter_pos = line.find_first_of(value_delimiter_, search_start + 1);
            const auto comment_pos = line.find_first_of(comment_char_, search_start);
            if (comment_pos < delimiter_pos)
            {
                delimiter_pos = std::string::npos;
            }

            if (delimiter_pos != std::string::npos)
            {
                name = detail::trim_copy(line.substr(0, delimiter_pos));
                std::string item = detail::trim_copy(line.substr(delimiter_pos + 1, std::string::npos));

                const bool ml_quote = (item.compare(0, 3, multiline_literal_quote) == 0 ||
                                       item.compare(0, 3, multiline_string_quote) == 0);

                if (!ml_quote && comment_pos != std::string::npos)
                {
                    const auto comment_items = detail::split_up(item, comment_char_);
                    item = detail::trim_copy(comment_items.front());
                }

                if (ml_quote)
                {
                    // Re-read the value from the untrimmed buffer, since leading
                    // whitespace inside a multi-line string is significant.
                    const auto key_char = item.front();
                    item = buffer.substr(delimiter_pos + 1, std::string::npos);
                    detail::ltrim(item);
                    item.erase(0, 3);

                    in_mline_value = true;
                    bool line_extension {false};
                    bool first_line = true;

                    if (!item.empty() && item.back() == '\\' && key_char == '\"')
                    {
                        item.pop_back();
                        line_extension = true;
                    }
                    else if (detail::has_ml_string(item, key_char))
                    {
                        // The opening line also closes the string.
                        item.resize(item.size() - 3);
                        if (key_char == '\"')
                        {
                            try
                            {
                                item = detail::remove_escaped_characters(item);
                            }
                            catch (const std::invalid_argument &iarg)
                            {
                                throw parse_error_t(iarg.what(), exit_codes_t::invalid_error);
                            }
                        }
                        in_mline_value = false;
                    }

                    while (in_mline_value)
                    {
                        std::string l2;
                        if (!std::getline(input, l2))
                        {
                            break;
                        }
                        line = l2;
                        detail::rtrim(line);

                        if (detail::has_ml_string(line, key_char))
                        {
                            line.resize(line.size() - 3);
                            if (line_extension)
                            {
                                detail::ltrim(line);
                            }
                            else if (!(first_line && item.empty()))
                            {
                                item.push_back('\n');
                            }
                            first_line = false;
                            item += line;
                            in_mline_value = false;
                            if (!item.empty() && item.back() == '\n')
                            {
                                item.pop_back();
                            }
                            if (key_char == '\"')
                            {
                                try
                                {
                                    item = detail::remove_escaped_characters(item);
                                }
                                catch (const std::invalid_argument &iarg)
                                {
                                    throw parse_error_t(iarg.what(), exit_codes_t::invalid_error);
                                }
                            }
                        }
                        else
                        {
                            if (line_extension)
                            {
                                detail::trim(l2);
                            }
                            else if (!(first_line && item.empty()))
                            {
                                item.push_back('\n');
                            }
                            line_extension = false;
                            first_line = false;
                            // A trailing backslash in an escaped string joins the next line.
                            if (!l2.empty() && l2.back() == '\\' && key_char == '\"')
                            {
                                line_extension = true;
                                l2.pop_back();
                            }
                            item += l2;
                        }
                    }
                    items_buffer = {item};
                }
                else if (!item.empty() && item.front() == a_start)
                {
                    // An array may span lines; keep reading until it closes.
                    for (std::string multiline; item.back() != a_end && std::getline(input, multiline);)
                    {
                        detail::trim(multiline);
                        item += multiline;
                    }
                    if (item.back() == a_end)
                    {
                        items_buffer = detail::split_up(item.substr(1, item.length() - 2), a_sep);
                    }
                    else
                    {
                        items_buffer = detail::split_up(item.substr(1, std::string::npos), a_sep);
                    }
                }
                else if ((is_default_array || is_ini_array) && item.find_first_of(a_sep) != std::string::npos)
                {
                    items_buffer = detail::split_up(item, a_sep);
                }
                else if ((is_default_array || is_ini_array) && item.find_first_of(' ') != std::string::npos)
                {
                    items_buffer = detail::split_up(item, '\0');
                }
                else
                {
                    items_buffer = {item};
                }
            }
            else
            {
                // A bare name with no delimiter is a flag.
                name = detail::trim_copy(line.substr(0, comment_pos));
                items_buffer = {"true"};
            }

            std::vector<std::string> parents;
            try
            {
                parents = detail::generate_parents(current_section, name, parent_separator_char_);
                detail::process_quoted_string(name, '"', '\'', true);
                for (auto &it : items_buffer)
                {
                    detail::process_quoted_string(it, string_quote_, literal_quote_);
                }
            }
            catch (const std::invalid_argument &ia)
            {
                throw parse_error_t(ia.what(), exit_codes_t::invalid_error);
            }

            if (parents.size() > maximum_layers_)
            {
                continue;
            }

            // When a specific section was requested, drop everything outside it and
            // strip the section itself off the parent path.
            if (!config_section_.empty() && !in_section)
            {
                if (parents.empty() || parents.front() != config_section_)
                {
                    continue;
                }
                if (config_index_ >= 0 && current_section_index != config_index_)
                {
                    continue;
                }
                parents.erase(parents.begin());
                in_section = true;
            }

            auto match = detail::find_matching_config(output, parents, name, allow_multiple_duplicate_fields_);
            if (match != output.end())
            {
                if ((match->inputs.size() > 1 && items_buffer.size() > 1) || allow_multiple_duplicate_fields_)
                {
                    // Mark the boundary between two appearances of the same field.
                    if (!(match->inputs.back().empty() || items_buffer.front().empty() ||
                          match->inputs.back() == "%%" || items_buffer.front() == "%%"))
                    {
                        match->inputs.emplace_back("%%");
                        match->multiline = true;
                    }
                }
                match->inputs.insert(match->inputs.end(), items_buffer.begin(), items_buffer.end());
            }
            else
            {
                output.emplace_back();
                output.back().parents = std::move(parents);
                output.back().name = std::move(name);
                output.back().inputs = std::move(items_buffer);
            }
        }

        // Close whatever section was still open at end of input.
        if (current_section != "default")
        {
            std::string ename;
            output.emplace_back();
            output.back().parents = detail::generate_parents(current_section, ename, parent_separator_char_);
            output.back().name = "--";
            while (output.back().parents.size() > 1)
            {
                output.push_back(output.back());
                output.back().parents.pop_back();
            }
        }

        return output;
    }

    /// @brief Quotes a name for writing when it contains anything structural.
    ///
    /// Prefers the literal form; falls back to the escaped form when the name
    /// already contains an apostrophe.
    ///
    /// @param[in,out] name The name to quote in place.
    /// @param[in] key_chars The characters that force quoting.
    /// @return A reference to @p name.
    auto clean_name_string(std::string &name, const std::string &key_chars) -> std::string &
    {
        if (name.find_first_of(key_chars) != std::string::npos || (name.front() == '[' && name.back() == ']') ||
            (name.find_first_of("'`\"\\") != std::string::npos))
        {
            if (name.find_first_of('\'') == std::string::npos)
            {
                name.insert(0, 1, '\'');
                name.push_back('\'');
            }
            else
            {
                if (detail::has_escapable_character(name))
                {
                    name = detail::add_escaped_characters(name);
                }
                name.insert(0, 1, '\"');
                name.push_back('\"');
            }
        }
        return name;
    }

    auto config_base_t::to_config(const app_t *app, bool default_also, bool write_description, std::string prefix) const
        -> std::string
    {
        return to_config(app,
                         default_also ? config_output_mode_t::all_defaults : config_output_mode_t::active,
                         write_description,
                         std::move(prefix));
    }

    auto config_base_t::to_config(const app_t *app,
                                  config_output_mode_t mode,
                                  bool write_description,
                                  std::string prefix) const -> std::string
    {
        std::ostringstream out;
        const bool include_default_values = (mode != config_output_mode_t::active);

        std::string comment_lead;
        comment_lead.push_back(comment_char_);
        comment_lead.push_back(' ');

        std::string comment_test = "#;";
        comment_test.push_back(comment_char_);
        comment_test.push_back(parent_separator_char_);

        // Any of these characters in a name forces it to be quoted.
        std::string key_chars = comment_test;
        key_chars.push_back(literal_quote_);
        key_chars.push_back(string_quote_);
        key_chars.push_back(array_start_);
        key_chars.push_back(array_end_);
        key_chars.push_back(value_delimiter_);
        key_chars.push_back(array_separator_);

        const auto join_values = [this](const std::vector<std::string> &values) {
            return detail::ini_join(values, array_separator_, array_start_, array_end_, string_quote_, literal_quote_);
        };

        std::vector<std::string> groups = app->get_groups();
        bool default_used = false;
        groups.insert(groups.begin(), std::string("OPTIONS"));

        for (auto &group : groups)
        {
            if (group == "OPTIONS" || group.empty())
            {
                if (default_used)
                {
                    continue;
                }
                default_used = true;
            }
            if (write_description && group != "OPTIONS" && !group.empty())
            {
                out << '\n' << comment_char_ << comment_lead << group << " Options\n";
            }

            for (const option_t *opt : app->get_options({}))
            {
                if (!opt->get_configurable())
                {
                    continue;
                }
                if (opt->get_group() != group)
                {
                    if (!(group == "OPTIONS" && opt->get_group().empty()))
                    {
                        continue;
                    }
                }

                std::string single_name = opt->get_single_name();
                if (single_name.empty())
                {
                    continue;
                }

                auto results = opt->reduced_results();
                if (results.size() > 1 && opt->get_multi_option_policy() == multi_option_policy_t::reverse)
                {
                    std::ranges::reverse(results);
                }

                // A summed option has already collapsed its inputs, and the sum may not
                // round-trip, so fall back to the raw results when it will not validate.
                if (opt->get_multi_option_policy() == multi_option_policy_t::sum && opt->count() >= 1 &&
                    results.size() == 1)
                {
                    const auto pos = opt->_validate(results[0], 0);
                    if (!pos.empty())
                    {
                        results = opt->results();
                    }
                }

                if (opt->get_multi_option_policy() == multi_option_policy_t::join && opt->count() > 1)
                {
                    const char delim = opt->get_delimiter();
                    if (delim == '\0')
                    {
                        // Joined with a newline, the value would not be readable back.
                        results = opt->results();
                    }
                    else
                    {
                        // Fall back when a value contains the delimiter itself, or when
                        // an empty element would be lost on re-reading.
                        const auto delim_count = std::ranges::count(results[0], delim);
                        if (results[0].back() == delim ||
                            static_cast<decltype(delim_count)>(opt->count()) < delim_count - 1 ||
                            results[0].find(std::string(2, delim)) != std::string::npos)
                        {
                            results = opt->results();
                        }
                    }
                }

                std::string value;
                if (opt->count() == 1 && results.size() == 2 && results.front() == "{}" && results.back() == "%%")
                {
                    // "{}" plus a sequence terminator is how an explicitly empty
                    // container is carried internally; written out it has to look like
                    // the literal string again.
                    value = "\"{}\"";
                }
                else
                {
                    value = join_values(results);
                }

                bool is_default = false;
                if (value.empty() && include_default_values)
                {
                    if (!opt->get_default_str().empty())
                    {
                        results_t res;
                        opt->results(res);
                        value = join_values(res);
                    }
                    else if (opt->get_expected_min() == 0)
                    {
                        value = "false";
                    }
                    else if (opt->get_run_callback_for_default() || !opt->get_required())
                    {
                        value = "\"\"";
                    }
                    else
                    {
                        value = "\"<REQUIRED>\"";
                    }
                    is_default = true;
                }

                if (value.empty())
                {
                    continue;
                }

                if (!opt->get_fnames().empty())
                {
                    try
                    {
                        value = opt->get_flag_value(single_name, value);
                    }
                    catch (const argument_mismatch_t &)
                    {
                        // The representative name rejected this value; try the other
                        // flag names before giving up and writing the raw results.
                        bool valid {false};
                        for (const auto &test_name : opt->get_fnames())
                        {
                            try
                            {
                                value = opt->get_flag_value(test_name, value);
                                single_name = test_name;
                                valid = true;
                            }
                            catch (const argument_mismatch_t &)
                            {
                                continue;
                            }
                        }
                        if (!valid)
                        {
                            value = join_values(opt->results());
                        }
                    }
                }

                if (write_description && opt->has_description())
                {
                    if (out.tellp() != std::streampos(0))
                    {
                        out << '\n';
                    }
                    out << comment_lead << detail::fix_newlines(comment_lead, opt->get_description()) << '\n';
                }

                clean_name_string(single_name, key_chars);

                std::string name = prefix + single_name;
                if (comment_defaults_bool_ && is_default)
                {
                    name = comment_char_ + name;
                }
                out << name << value_delimiter_ << value << '\n';
            }
        }

        const auto subcommands = app->get_subcommands({});

        // A nameless subcommand is an option group: emit its options inline, under the
        // same section as the parent.
        for (const app_t *subcom : subcommands)
        {
            if (!subcom->get_name().empty())
            {
                continue;
            }
            if (!include_default_values && (subcom->count_all() == 0))
            {
                continue;
            }
            if (write_description && !subcom->get_group().empty())
            {
                out << '\n' << comment_lead << subcom->get_group() << " Options\n";
            }
            out << to_config(subcom, mode, write_description, prefix);
        }

        for (const app_t *subcom : subcommands)
        {
            if (subcom->get_name().empty())
            {
                continue;
            }
            if ((!include_default_values && (subcom->count_all() == 0)) ||
                (mode == config_output_mode_t::active_subcommand_defaults && !app->got_subcommand(subcom)))
            {
                continue;
            }

            std::string subname = subcom->get_name();
            clean_name_string(subname, key_chars);

            if (subcom->get_configurable() &&
                (app->got_subcommand(subcom) || (mode == config_output_mode_t::all_defaults)))
            {
                // A configurable subcommand gets its own section header. Build the full
                // dotted path by walking up to the root.
                if (!prefix.empty() || app->get_parent() == nullptr)
                {
                    out << '[' << prefix << subname << "]\n";
                }
                else
                {
                    std::string appname = app->get_name();
                    clean_name_string(appname, key_chars);
                    subname = appname + parent_separator_char_ + subname;

                    const auto *p = app->get_parent();
                    while (p->get_parent() != nullptr)
                    {
                        std::string pname = p->get_name();
                        clean_name_string(pname, key_chars);
                        subname = pname + parent_separator_char_ + subname;
                        p = p->get_parent();
                    }
                    out << '[' << subname << "]\n";
                }
                out << to_config(subcom, mode, write_description, "");
            }
            else
            {
                // Not configurable as a section, so its options are written with a
                // dotted prefix instead.
                out << to_config(subcom, mode, write_description, prefix + subname + parent_separator_char_);
            }
        }

        if (write_description && !out.str().empty())
        {
            const std::string out_string = comment_char_ + comment_lead +
                                           detail::fix_newlines(comment_char_ + comment_lead, app->get_description()) +
                                           '\n';
            return out_string + out.str();
        }
        return out.str();
    }

} // namespace cli
