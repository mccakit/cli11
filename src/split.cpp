/// @file
/// @brief Decomposition of command-line arguments and name specifications.
///
/// Two separate jobs live here. The `split_*` functions take one argument off
/// the command line and pull it apart into a name and a value. The `get_names`
/// and `get_default_flag_values` functions take a name specification as written
/// by the caller — something like `"-f,--file,filename"` — and work out what
/// was meant.

export module cli11:split;

import std;
import :error;
import :string_tools;

export namespace cli
{

    namespace detail
    {

        /// @brief One argument pulled apart into a name and the text following it.
        struct split_result_t
        {
                /// @brief The option name, with any leading dashes or slash removed.
                std::string name;

                /// @brief The text that followed the name.
                ///
                /// For @ref split_long and @ref split_windows_style this is the value
                /// after the separator, empty when no separator was present. For
                /// @ref split_short it is the remainder of the argument, which may hold
                /// further bundled short options rather than a value.
                std::string value;
        };

        /// @brief Splits a short-form argument such as `-fvalue`.
        ///
        /// @param current The argument to split.
        /// @return The single-character name and the remainder of the argument, or
        /// `std::nullopt` if @p current is not a short-form argument.
        [[nodiscard]] auto split_short(const std::string &current) -> std::optional<split_result_t>
        {
            if (current.size() > 1 && current[0] == '-' && valid_first_char(current[1]))
            {
                return split_result_t {current.substr(1, 1), current.substr(2)};
            }
            return std::nullopt;
        }

        /// @brief Splits a long-form argument such as `--file=value`.
        ///
        /// @param current The argument to split.
        /// @return The name and the value after `=`, or `std::nullopt` if @p current
        /// is not a long-form argument. The value is empty when there is no `=`.
        [[nodiscard]] auto split_long(const std::string &current) -> std::optional<split_result_t>
        {
            if (current.size() > 2 && current.compare(0, 2, "--") == 0 && valid_first_char(current[2]))
            {
                const auto loc = current.find_first_of('=');
                if (loc != std::string::npos)
                {
                    return split_result_t {current.substr(2, loc - 2), current.substr(loc + 1)};
                }
                return split_result_t {current.substr(2), {}};
            }
            return std::nullopt;
        }

        /// @brief Splits a Windows-style argument such as `/file:value`.
        ///
        /// @param current The argument to split.
        /// @return The name and the value after `:`, or `std::nullopt` if @p current
        /// is not a Windows-style argument. The value is empty when there is no `:`.
        [[nodiscard]] auto split_windows_style(const std::string &current) -> std::optional<split_result_t>
        {
            if (current.size() > 1 && current[0] == '/' && valid_first_char(current[1]))
            {
                const auto loc = current.find_first_of(':');
                if (loc != std::string::npos)
                {
                    return split_result_t {current.substr(1, loc - 1), current.substr(loc + 1)};
                }
                return split_result_t {current.substr(1), {}};
            }
            return std::nullopt;
        }

        /// @brief Splits a comma-separated name specification, trimming each entry.
        ///
        /// @param current The specification to split.
        /// @return The individual names, each trimmed of surrounding whitespace.
        [[nodiscard]] auto split_names(std::string current) -> std::vector<std::string>
        {
            std::vector<std::string> output;
            std::size_t val = 0;
            while ((val = current.find(',')) != std::string::npos)
            {
                output.push_back(trim_copy(current.substr(0, val)));
                current = current.substr(val + 1);
            }
            output.push_back(trim_copy(current));
            return output;
        }

        /// @brief Extracts the default values written into a flag specification.
        ///
        /// Recognises `name{value}` for an explicit default and a leading `!` for a
        /// negated flag, which defaults to `"false"`. Entries carrying neither are
        /// skipped.
        ///
        /// @param str The flag specification, for example `"--flag{7},!--no-flag"`.
        /// @return Each flag name paired with its default value.
        [[nodiscard]] auto get_default_flag_values(const std::string &str)
            -> std::vector<std::pair<std::string, std::string>>
        {
            std::vector<std::string> flags = split_names(str);
            std::erase_if(flags, [](const std::string &name) {
                return ((name.empty()) || (!(((name.find_first_of('{') != std::string::npos) && (name.back() == '}')) ||
                                             (name[0] == '!'))));
            });

            std::vector<std::pair<std::string, std::string>> output;
            output.reserve(flags.size());
            for (auto &flag : flags)
            {
                const auto def_start = flag.find_first_of('{');
                std::string defval = "false";
                if ((def_start != std::string::npos) && (flag.back() == '}'))
                {
                    defval = flag.substr(def_start + 1);
                    defval.pop_back();
                    flag.erase(def_start, std::string::npos);
                }
                flag.erase(0, flag.find_first_not_of("-!"));
                output.emplace_back(flag, defval);
            }
            return output;
        }

        /// @brief The names extracted from a name specification, sorted by kind.
        struct option_names_t
        {
                /// @brief Short names, without their leading dash.
                std::vector<std::string> short_names;

                /// @brief Long names, without their leading dashes.
                std::vector<std::string> long_names;

                /// @brief The positional name, empty if none was given.
                std::string positional_name;
        };

        /// @brief Sorts a name specification into short, long, and positional names.
        ///
        /// @param input The individual names, as produced by @ref split_names.
        /// @param allow_non_standard Accept multi-character short names such as `-abc`.
        /// @return The names, sorted by kind.
        /// @throws cli::bad_name_string_t If a name is malformed, reserved, or if more
        /// than one positional name is given.
        [[nodiscard]] auto get_names(const std::vector<std::string> &input, bool allow_non_standard = false)
            -> option_names_t
        {
            option_names_t output;

            for (std::string name : input)
            {
                if (name.empty())
                {
                    continue;
                }
                if (name.length() > 1 && name[0] == '-' && name[1] != '-')
                {
                    if (name.length() == 2 && valid_first_char(name[1]))
                    {
                        output.short_names.emplace_back(1, name[1]);
                    }
                    else if (name.length() > 2)
                    {
                        if (allow_non_standard)
                        {
                            name = name.substr(1);
                            if (valid_name_string(name))
                            {
                                output.short_names.push_back(name);
                            }
                            else
                            {
                                throw bad_name_string_t::bad_long_name(name);
                            }
                        }
                        else
                        {
                            throw bad_name_string_t::missing_dash(name);
                        }
                    }
                    else
                    {
                        throw bad_name_string_t::one_char_name(name);
                    }
                }
                else if (name.length() > 2 && name.substr(0, 2) == "--")
                {
                    name = name.substr(2);
                    if (valid_name_string(name))
                    {
                        output.long_names.push_back(name);
                    }
                    else
                    {
                        throw bad_name_string_t::bad_long_name(name);
                    }
                }
                else if (name == "-" || name == "--" || name == "++")
                {
                    throw bad_name_string_t::reserved_name(name);
                }
                else
                {
                    if (!output.positional_name.empty())
                    {
                        throw bad_name_string_t::multi_positional_names(name);
                    }
                    if (valid_name_string(name))
                    {
                        output.positional_name = name;
                    }
                    else
                    {
                        throw bad_name_string_t::bad_positional_name(name);
                    }
                }
            }
            return output;
        }

    } // namespace detail

} // namespace cli
