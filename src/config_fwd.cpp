// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

/// @file
/// @brief Declarations for configuration-file readers and writers.
///
/// A configuration converter turns an @ref cli::app_t into text and back again.
/// @ref cli::config_t is the interface; @ref cli::config_base_t implements it for
/// TOML-shaped files and exposes the punctuation as settings, so
/// @ref cli::config_ini_t is the same reader with different characters.
///
/// The implementations live in the `config` partition. This one is separate so
/// that `app_t` can hold a config converter without a circular dependency.

export module cli11:config_fwd;

import std;
import :encoding;
import :error;
import :string_tools;

export namespace cli
{

    class app_t;

    /// @brief How much of an application is written out to a configuration file.
    enum class config_output_mode_t : std::uint8_t
    {
        active = 0,                ///< Only options that were actually set.
        all_defaults,              ///< Every option, including untouched defaults.
        active_subcommand_defaults ///< Defaults, but only for subcommands that were used.
    };

    /// @brief One entry read from a configuration file.
    struct config_item_t
    {
            /// @brief The section path leading to this entry.
            std::vector<std::string> parents {};

            /// @brief The entry's own name.
            std::string name {};

            /// @brief The values given for this entry.
            std::vector<std::string> inputs {};

            /// @brief Whether a multiline vector separator was inserted.
            bool multiline {false};

            /// @brief Returns the section path and name joined by `.`.
            ///
            /// @return The fully qualified name.
            [[nodiscard]] auto fullname() const -> std::string
            {
                (void)multiline; // suppression for cppcheck false positive
                std::vector<std::string> tmp = parents;
                tmp.emplace_back(name);
                return detail::join(tmp, ".");
            }
    };

    /// @brief Interface for converting between an application and a configuration file.
    class config_t
    {
        protected:
            /// @brief Entries accumulated while reading.
            std::vector<config_item_t> items {};

        public:
            config_t() = default;
            config_t(const config_t &) = default;
            config_t(config_t &&) = default;
            auto operator=(const config_t &) -> config_t & = default;
            auto operator=(config_t &&) -> config_t & = default;

            /// @brief Writes an application out as configuration text.
            ///
            /// @param app The application to describe.
            /// @param default_also Include options left at their defaults.
            /// @param write_description Include descriptions as comments.
            /// @param prefix Section prefix to write entries under.
            /// @return The configuration text.
            [[nodiscard]] virtual auto to_config(const app_t *app,
                                                 bool default_also,
                                                 bool write_description,
                                                 std::string prefix) const -> std::string = 0;

            /// @brief Writes an application out as configuration text.
            ///
            /// @param app The application to describe.
            /// @param mode How much of the application to write.
            /// @param write_description Include descriptions as comments.
            /// @param prefix Section prefix to write entries under.
            /// @return The configuration text.
            [[nodiscard]] virtual auto to_config(const app_t *app,
                                                 config_output_mode_t mode,
                                                 bool write_description,
                                                 std::string prefix) const -> std::string
            {
                return to_config(app, mode != config_output_mode_t::active, write_description, std::move(prefix));
            }

            /// @brief Reads configuration text into a list of entries.
            ///
            /// @param input The stream to read from.
            /// @return The entries that were read.
            [[nodiscard]] virtual auto from_config(std::istream &input) const -> std::vector<config_item_t> = 0;

            /// @brief Reduces an entry to a single flag value.
            ///
            /// @param item The entry to reduce.
            /// @return The single value, or `"{}"` if the entry has none.
            /// @throws cli::conversion_error_t If the entry has more than one value.
            [[nodiscard]] virtual auto to_flag(const config_item_t &item) const -> std::string
            {
                if (item.inputs.size() == 1)
                {
                    return item.inputs.at(0);
                }
                if (item.inputs.empty())
                {
                    return "{}";
                }
                throw conversion_error_t::too_many_inputs_flag(item.fullname()); // LCOV_EXCL_LINE
            }

            /// @brief Reads a configuration file into a list of entries.
            ///
            /// @param name Path to the file.
            /// @return The entries that were read.
            /// @throws cli::file_error_t If the file cannot be opened.
            /// @throws cli::config_error_t If the contents cannot be parsed.
            [[nodiscard]] auto from_file(const std::string &name) const -> std::vector<config_item_t>
            {
                std::ifstream input {to_path(name)};

                if (!input.good())
                {
                    throw file_error_t::missing(name);
                }

                return from_config(input);
            }

            virtual ~config_t() = default;
    };

    /// @brief A configuration converter for INI and TOML files.
    ///
    /// Every piece of punctuation is a setting, so the same reader handles both
    /// dialects. @ref cli::config_ini_t is this class with INI characters preset.
    class config_base_t : public config_t
    {
        protected:
            /// @brief The character introducing a comment.
            char comment_char_ {'#'};

            /// @brief The character opening an array; `'\0'` disables arrays.
            char array_start_ {'['};

            /// @brief The character closing an array; `'\0'` disables arrays.
            char array_end_ {']'};

            /// @brief The character separating elements within an array.
            char array_separator_ {','};

            /// @brief The character separating a name from its value.
            char value_delimiter_ {'='};

            /// @brief The character quoting escaped strings.
            char string_quote_ {'"'};

            /// @brief The character quoting literal strings and single characters.
            char literal_quote_ {'\''};

            /// @brief The maximum number of nested sections to descend into.
            std::uint8_t maximum_layers_ {255};

            /// @brief The character separating nested section names.
            char parent_separator_char_ {'.'};

            /// @brief Whether default values are written out commented.
            bool comment_defaults_bool_ {false};

            /// @brief Whether repeated field names collapse into a single vector.
            bool allow_multiple_duplicate_fields_ {false};

            /// @brief Which index of an arrayed section to use; `-1` means all.
            std::int16_t config_index_ {-1};

            /// @brief Which section of the file to use; empty means the whole file.
            std::string config_section_ {};

        public:
            [[nodiscard]] auto to_config(const app_t *app,
                                         config_output_mode_t mode,
                                         bool write_description,
                                         std::string prefix) const -> std::string override;

            [[nodiscard]] auto to_config(const app_t *app,
                                         bool default_also,
                                         bool write_description,
                                         std::string prefix) const -> std::string override;

            [[nodiscard]] auto from_config(std::istream &input) const -> std::vector<config_item_t> override;

            /// @brief Sets the character introducing a comment.
            ///
            /// @param cchar The comment character.
            /// @return A pointer to this converter, for chaining.
            auto comment(char cchar) -> config_base_t *
            {
                comment_char_ = cchar;
                return this;
            }

            /// @brief Sets the characters delimiting an array.
            ///
            /// @param a_start The opening character.
            /// @param a_end The closing character.
            /// @return A pointer to this converter, for chaining.
            auto array_bounds(char a_start, char a_end) -> config_base_t *
            {
                array_start_ = a_start;
                array_end_ = a_end;
                return this;
            }

            /// @brief Sets the character separating array elements.
            ///
            /// @param a_sep The separator character.
            /// @return A pointer to this converter, for chaining.
            auto array_delimiter(char a_sep) -> config_base_t *
            {
                array_separator_ = a_sep;
                return this;
            }

            /// @brief Sets the character separating a name from its value.
            ///
            /// @param v_sep The separator character.
            /// @return A pointer to this converter, for chaining.
            auto value_separator(char v_sep) -> config_base_t *
            {
                value_delimiter_ = v_sep;
                return this;
            }

            /// @brief Sets the quote characters.
            ///
            /// @param q_string The character quoting escaped strings.
            /// @param literal_char The character quoting literal strings.
            /// @return A pointer to this converter, for chaining.
            auto quote_character(char q_string, char literal_char) -> config_base_t *
            {
                string_quote_ = q_string;
                literal_quote_ = literal_char;
                return this;
            }

            /// @brief Sets how many nested sections to descend into.
            ///
            /// @param layers The maximum depth.
            /// @return A pointer to this converter, for chaining.
            auto max_layers(std::uint8_t layers) -> config_base_t *
            {
                maximum_layers_ = layers;
                return this;
            }

            /// @brief Sets the character separating nested section names.
            ///
            /// @param sep The separator character.
            /// @return A pointer to this converter, for chaining.
            auto parent_separator(char sep) -> config_base_t *
            {
                parent_separator_char_ = sep;
                return this;
            }

            /// @brief Sets whether default values are written out commented.
            ///
            /// @param com_def Whether to comment defaults.
            /// @return A pointer to this converter, for chaining.
            auto comment_defaults(bool com_def = true) -> config_base_t *
            {
                comment_defaults_bool_ = com_def;
                return this;
            }

            /// @brief Returns a mutable reference to the selected section name.
            ///
            /// @return A reference to the section name.
            [[nodiscard]] auto section_ref() -> std::string &
            {
                return config_section_;
            }

            /// @brief Returns the selected section name.
            ///
            /// @return The section name, empty if the whole file is used.
            [[nodiscard]] auto section() const -> const std::string &
            {
                return config_section_;
            }

            /// @brief Selects a particular section of the file.
            ///
            /// @param section_name The section to use.
            /// @return A pointer to this converter, for chaining.
            auto section(const std::string &section_name) -> config_base_t *
            {
                config_section_ = section_name;
                return this;
            }

            /// @brief Returns a mutable reference to the selected section index.
            ///
            /// @return A reference to the section index.
            [[nodiscard]] auto index_ref() -> std::int16_t &
            {
                return config_index_;
            }

            /// @brief Returns the selected section index.
            ///
            /// @return The index, or `-1` if every section is used.
            [[nodiscard]] auto index() const -> std::int16_t
            {
                return config_index_;
            }

            /// @brief Selects a particular index within an arrayed section.
            ///
            /// @param section_index The index to use, or `-1` for all sections.
            /// @return A pointer to this converter, for chaining.
            auto index(std::int16_t section_index) -> config_base_t *
            {
                config_index_ = section_index;
                return this;
            }

            /// @brief Sets whether non-sequential duplicate fields are merged.
            ///
            /// @param value Whether to merge duplicates.
            /// @return A pointer to this converter, for chaining.
            auto allow_duplicate_fields(bool value = true) -> config_base_t *
            {
                allow_multiple_duplicate_fields_ = value;
                return this;
            }
    };

    /// @brief The default configuration format, which is TOML.
    using config_toml_t = config_base_t;

    /// @brief A configuration converter producing standard INI output.
    class config_ini_t : public config_toml_t
    {
        public:
            /// @brief Presets the punctuation for INI files.
            config_ini_t()
            {
                comment_char_ = ';';
                array_start_ = '\0';
                array_end_ = '\0';
                array_separator_ = ' ';
                value_delimiter_ = '=';
            }
    };

} // namespace cli
