/// @file
/// @brief The validator type and the built-in validators.
///
/// A @ref cli::validator_t wraps a callable that inspects — and may rewrite — one
/// value, returning an empty string on success or a message on failure. Attach
/// one with `option->check(...)` or `option->transform(...)`.
///
/// Validators compose. `&` requires both to pass, `|` requires either, and `!`
/// inverts, with the descriptions combined to match:
///
/// @code
/// app.add_option("--f", file)->check(cli::existing_file & !cli::existing_directory);
/// @endcode
///
/// The heavier validators — `is_member`, the transformers, and the unit parsers —
/// live in the `extra_validators` partition.

export module cli11:validators;

import std;
import :error;
import :string_tools;
import :type_tools;
import :encoding;

export namespace cli
{

    class option_t;

    /// @brief A composable check applied to one command-line value.
    class validator_t
    {
        protected:
            /// @brief Produces the description shown in help output.
            ///
            /// A callable rather than a string so that a description can be built
            /// lazily from the descriptions of composed validators.
            std::function<std::string()> desc_function_ {[] { return std::string {}; }};

            /// @brief The check itself.
            ///
            /// Returns an empty string on success, or a failure message. May rewrite
            /// its argument unless @ref non_modifying_ is set.
            std::function<std::string(std::string &)> func_ {[](std::string &)
                                                                            { return std::string {}; }};

            /// @brief The validator's name, used to find and replace it on an option.
            std::string name_ {};

            /// @brief Which value this applies to, or `-1` for all of them.
            int application_index_ = -1;

            /// @brief Whether the check runs at all.
            bool active_ {true};

            /// @brief Whether the check is forbidden from rewriting its argument.
            bool non_modifying_ {false};

            /// @brief Constructs a validator with a fixed description.
            ///
            /// @param validator_desc The description shown in help output.
            /// @param func The check to run.
            validator_t(std::string validator_desc, std::function<std::string(std::string &)> func)
                : desc_function_([desc = std::move(validator_desc)] { return desc; }), func_(std::move(func))
            {
            }

        public:
            validator_t() = default;

            /// @brief Constructs a validator that only carries a description.
            ///
            /// @param validator_desc The description shown in help output.
            explicit validator_t(std::string validator_desc)
                : desc_function_([desc = std::move(validator_desc)] { return desc; })
            {
            }

            /// @brief Constructs a named validator.
            ///
            /// @param op The check to run.
            /// @param validator_desc The description shown in help output.
            /// @param validator_name The name used to find this validator later.
            validator_t(std::function<std::string(std::string &)> op, std::string validator_desc,
                        std::string validator_name = "")
                : desc_function_([desc = std::move(validator_desc)] { return desc; }), func_(std::move(op)),
                  name_(std::move(validator_name))
            {
            }

            /// @brief Replaces the check.
            ///
            /// @param op The new check.
            /// @return A reference to this validator, for chaining.
            auto operation(std::function<std::string(std::string &)> op) -> validator_t &
            {
                func_ = std::move(op);
                return *this;
            }

            /// @brief Runs the check, possibly rewriting @p str.
            ///
            /// @param[in,out] str The value to check.
            /// @return An empty string on success, otherwise a failure message.
            auto operator()(std::string &str) const -> std::string
            {
                std::string retstring;
                if (active_)
                {
                    if (non_modifying_)
                    {
                        std::string value = str;
                        retstring = func_(value);
                    }
                    else
                    {
                        retstring = func_(str);
                    }
                }
                return retstring;
            }

            /// @brief Runs the check against a copy, leaving @p str untouched.
            ///
            /// @param str The value to check.
            /// @return An empty string on success, otherwise a failure message.
            auto operator()(const std::string &str) const -> std::string
            {
                std::string value = str;
                return (active_) ? func_(value) : std::string {};
            }

            /// @brief Sets the description in place.
            ///
            /// @param validator_desc The new description.
            /// @return A reference to this validator, for chaining.
            auto description(std::string validator_desc) -> validator_t &
            {
                desc_function_ = [desc = std::move(validator_desc)] { return desc; };
                return *this;
            }

            /// @brief Returns a copy carrying a different description.
            ///
            /// @param validator_desc The new description.
            /// @return The modified copy.
            [[nodiscard]] auto description(std::string validator_desc) const -> validator_t
            {
                validator_t newval(*this);
                newval.desc_function_ = [desc = std::move(validator_desc)] { return desc; };
                return newval;
            }

            /// @brief Returns the description shown in help output.
            ///
            /// @return The description, or an empty string if the validator is inactive.
            [[nodiscard]] auto get_description() const -> std::string
            {
                if (active_)
                {
                    return desc_function_();
                }
                return std::string {};
            }

            /// @brief Sets the name in place.
            ///
            /// @param validator_name The new name.
            /// @return A reference to this validator, for chaining.
            auto name(std::string validator_name) -> validator_t &
            {
                name_ = std::move(validator_name);
                return *this;
            }

            /// @brief Returns a copy carrying a different name.
            ///
            /// @param validator_name The new name.
            /// @return The modified copy.
            [[nodiscard]] auto name(std::string validator_name) const -> validator_t
            {
                validator_t newval(*this);
                newval.name_ = std::move(validator_name);
                return newval;
            }

            /// @brief Returns the validator's name.
            ///
            /// @return The name.
            [[nodiscard]] auto get_name() const -> const std::string &
            {
                return name_;
            }

            /// @brief Enables or disables the check in place.
            ///
            /// @param active_val Whether the check should run.
            /// @return A reference to this validator, for chaining.
            auto active(bool active_val = true) -> validator_t &
            {
                active_ = active_val;
                return *this;
            }

            /// @brief Returns a copy that is enabled or disabled.
            ///
            /// @param active_val Whether the check should run.
            /// @return The modified copy.
            [[nodiscard]] auto active(bool active_val = true) const -> validator_t
            {
                validator_t newval(*this);
                newval.active_ = active_val;
                return newval;
            }

            /// @brief Reports whether the check runs.
            ///
            /// @return `true` if the validator is active.
            [[nodiscard]] auto get_active() const -> bool
            {
                return active_;
            }

            /// @brief Forbids or permits the check to rewrite its argument.
            ///
            /// @param no_modify Whether to forbid rewriting.
            /// @return A reference to this validator, for chaining.
            auto non_modifying(bool no_modify = true) -> validator_t &
            {
                non_modifying_ = no_modify;
                return *this;
            }

            /// @brief Reports whether the check may rewrite its argument.
            ///
            /// @return `true` if rewriting is permitted.
            [[nodiscard]] auto get_modifying() const -> bool
            {
                return !non_modifying_;
            }

            /// @brief Sets which value the check applies to, in place.
            ///
            /// @param app_index The value index, or `-1` for all values.
            /// @return A reference to this validator, for chaining.
            auto application_index(int app_index) -> validator_t &
            {
                application_index_ = app_index;
                return *this;
            }

            /// @brief Returns a copy applying to a different value.
            ///
            /// @param app_index The value index, or `-1` for all values.
            /// @return The modified copy.
            [[nodiscard]] auto application_index(int app_index) const -> validator_t
            {
                validator_t newval(*this);
                newval.application_index_ = app_index;
                return newval;
            }

            /// @brief Returns which value the check applies to.
            ///
            /// @return The value index, or `-1` for all values.
            [[nodiscard]] auto get_application_index() const -> int
            {
                return application_index_;
            }

            /// @brief Combines two validators so that both must pass.
            ///
            /// @param other The validator to combine with.
            /// @return The combined validator.
            auto operator&(const validator_t &other) const -> validator_t
            {
                validator_t newval;
                newval._merge_description(*this, other, " AND ");

                newval.func_ = [f1 = func_, f2 = other.func_](std::string &input) {
                    std::string s1 = f1(input);
                    std::string s2 = f2(input);
                    if (!s1.empty() && !s2.empty())
                    {
                        return std::string("(") + s1 + ") AND (" + s2 + ")";
                    }
                    return s1 + s2;
                };

                newval.active_ = active_ && other.active_;
                newval.application_index_ = application_index_;
                return newval;
            }

            /// @brief Combines two validators so that either may pass.
            ///
            /// @param other The validator to combine with.
            /// @return The combined validator.
            auto operator|(const validator_t &other) const -> validator_t
            {
                validator_t newval;
                newval._merge_description(*this, other, " OR ");

                newval.func_ = [f1 = func_, f2 = other.func_](std::string &input) {
                    std::string s1 = f1(input);
                    std::string s2 = f2(input);
                    if (s1.empty() || s2.empty())
                    {
                        return std::string();
                    }
                    return std::string("(") + s1 + ") OR (" + s2 + ")";
                };

                newval.active_ = active_ && other.active_;
                newval.application_index_ = application_index_;
                return newval;
            }

            /// @brief Inverts a validator, so that passing becomes failing.
            ///
            /// @return The inverted validator.
            auto operator!() const -> validator_t
            {
                validator_t newval;

                newval.desc_function_ = [dfunc1 = desc_function_] {
                    auto str = dfunc1();
                    return (!str.empty()) ? std::string("NOT ") + str : std::string {};
                };

                newval.func_ = [f1 = func_, dfunc1 = desc_function_](std::string &test) -> std::string {
                    const std::string s1 = f1(test);
                    if (s1.empty())
                    {
                        return std::string("check ") + dfunc1() + " succeeded improperly";
                    }
                    return std::string {};
                };

                newval.active_ = active_;
                newval.application_index_ = application_index_;
                return newval;
            }

        private:
            /// @brief Builds a combined description from two validators.
            ///
            /// @param val1 The left-hand validator.
            /// @param val2 The right-hand validator.
            /// @param merger The text placed between the two descriptions.
            auto _merge_description(const validator_t &val1, const validator_t &val2,
                                    const std::string &merger) -> void
            {
                desc_function_ = [dfunc1 = val1.desc_function_, dfunc2 = val2.desc_function_, merger] {
                    const std::string f1 = dfunc1();
                    const std::string f2 = dfunc2();
                    if ((f1.empty()) || (f2.empty()))
                    {
                        return f1 + f2;
                    }
                    return std::string(1, '(') + f1 + ')' + merger + '(' + f2 + ')';
                };
            }
    };

    /// @brief A validator built from a user-supplied callable.
    using custom_validator_t = validator_t;

    namespace detail
    {

        /// @brief What a path refers to on disk.
        enum class path_type_t : std::uint8_t
        {
            nonexistent, ///< Nothing exists at the path.
            file,        ///< Something other than a directory exists at the path.
            directory    ///< A directory exists at the path.
        };

        /// @brief Reports what a path refers to.
        ///
        /// @param file The path to inspect.
        /// @return What the path refers to; @ref path_type_t::nonexistent if it
        /// cannot be inspected at all.
        auto check_path(std::string_view file) noexcept -> path_type_t
        {
            std::error_code ec;
            const auto stat = std::filesystem::status(to_path(file), ec);
            if (ec)
            {
                return path_type_t::nonexistent;
            }
            switch (stat.type())
            {
            case std::filesystem::file_type::none:
            case std::filesystem::file_type::not_found:
                return path_type_t::nonexistent;
            case std::filesystem::file_type::directory:
                return path_type_t::directory;
            case std::filesystem::file_type::symlink:
            case std::filesystem::file_type::block:
            case std::filesystem::file_type::character:
            case std::filesystem::file_type::fifo:
            case std::filesystem::file_type::socket:
            case std::filesystem::file_type::regular:
            case std::filesystem::file_type::unknown:
            default:
                return path_type_t::file;
            }
        }

    } // namespace detail

    /// @brief Requires the value to name an existing file.
    const validator_t existing_file {[](std::string &filename) {
                                         const auto path_result = detail::check_path(filename);
                                         if (path_result == detail::path_type_t::nonexistent)
                                         {
                                             return "File does not exist: " + filename;
                                         }
                                         if (path_result == detail::path_type_t::directory)
                                         {
                                             return "File is actually a directory: " + filename;
                                         }
                                         return std::string();
                                     },
                                     "FILE"};

    /// @brief Requires the value to name an existing directory.
    const validator_t existing_directory {[](std::string &filename) {
                                              const auto path_result = detail::check_path(filename);
                                              if (path_result == detail::path_type_t::nonexistent)
                                              {
                                                  return "Directory does not exist: " + filename;
                                              }
                                              if (path_result == detail::path_type_t::file)
                                              {
                                                  return "Directory is actually a file: " + filename;
                                              }
                                              return std::string();
                                          },
                                          "DIR"};

    /// @brief Requires the value to name something that exists.
    const validator_t existing_path {[](std::string &filename) {
                                         const auto path_result = detail::check_path(filename);
                                         if (path_result == detail::path_type_t::nonexistent)
                                         {
                                             return "Path does not exist: " + filename;
                                         }
                                         return std::string();
                                     },
                                     "PATH(existing)"};

    /// @brief Requires the value to name something that does not exist.
    const validator_t nonexistent_path {[](std::string &filename) {
                                            const auto path_result = detail::check_path(filename);
                                            if (path_result != detail::path_type_t::nonexistent)
                                            {
                                                return "Path already exists: " + filename;
                                            }
                                            return std::string();
                                        },
                                        "PATH(non-existing)"};

    /// @brief Resolves quoting and backslash escapes in the value.
    ///
    /// This rewrites its argument rather than only inspecting it.
    const validator_t escaped_string {[](std::string &str) {
                                          try
                                          {
                                              if (str.size() > 1 &&
                                                  (str.front() == '\"' || str.front() == '\'' || str.front() == '`') &&
                                                  str.front() == str.back())
                                              {
                                                  detail::process_quoted_string(str);
                                              }
                                              else if (str.find_first_of('\\') != std::string::npos)
                                              {
                                                  if (detail::is_binary_escaped_string(str))
                                                  {
                                                      str = detail::extract_binary_string(str);
                                                  }
                                                  else
                                                  {
                                                      str = detail::remove_escaped_characters(str);
                                                  }
                                              }
                                              return std::string {};
                                          }
                                          catch (const std::invalid_argument &ia)
                                          {
                                              return std::string(ia.what());
                                          }
                                      },
                                      std::string {}};

    /// @brief Finds a file relative to a default directory when it is not found as given.
    ///
    /// On a match the value is rewritten to the resolved path.
    class file_on_default_path_t : public validator_t
    {
        public:
            /// @brief Constructs the validator.
            ///
            /// @param default_path The directory to search when the value does not resolve.
            /// @param enable_error_return Report a failure when neither path resolves.
            explicit file_on_default_path_t(std::string default_path, bool enable_error_return = true)
                : validator_t("FILE")
            {
                func_ = [default_path = std::move(default_path), enable_error_return](std::string &filename) {
                    auto path_result = detail::check_path(filename);
                    if (path_result == detail::path_type_t::nonexistent)
                    {
                        std::string test_file_path = default_path;
                        if (default_path.back() != '/' && default_path.back() != '\\')
                        {
                            test_file_path += '/';
                        }
                        test_file_path.append(filename);
                        path_result = detail::check_path(test_file_path);
                        if (path_result == detail::path_type_t::file)
                        {
                            filename = test_file_path;
                        }
                        else
                        {
                            if (enable_error_return)
                            {
                                return "File does not exist: " + filename;
                            }
                        }
                    }
                    return std::string {};
                };
            }
    };

    /// @brief Requires the value to fall within a closed interval.
    class range_t : public validator_t
    {
        public:
            /// @brief Constructs a validator for the interval `[min_val, max_val]`.
            ///
            /// @tparam T The type the value is converted to before comparison.
            /// @param min_val The lowest permitted value.
            /// @param max_val The highest permitted value.
            /// @param validator_name The validator's name; the description is
            /// generated when this is empty.
            template <typename T>
            range_t(T min_val, T max_val, const std::string &validator_name = std::string {})
                : validator_t(validator_name)
            {
                if (validator_name.empty())
                {
                    std::ostringstream out;
                    out << detail::type_name<T>() << " in [" << min_val << " - " << max_val << "]";
                    description(out.str());
                }
                func_ = [min_val, max_val](std::string &input) {
                    using detail::lexical_cast;
                    T val;
                    const bool converted = lexical_cast(input, val);
                    if ((!converted) || (val < min_val || val > max_val))
                    {
                        std::ostringstream out;
                        out << "Value " << input << " not in range [";
                        out << min_val << " - " << max_val << "]";
                        return out.str();
                    }
                    return std::string {};
                };
            }

            /// @brief Constructs a validator for the interval `[0, max_val]`.
            ///
            /// @tparam T The type the value is converted to before comparison.
            /// @param max_val The highest permitted value.
            /// @param validator_name The validator's name.
            template <typename T>
            explicit range_t(T max_val, const std::string &validator_name = std::string {})
                : range_t(static_cast<T>(0), max_val, validator_name)
            {
            }
    };

    /// @brief Requires the value to be zero or greater.
    const range_t non_negative_number((std::numeric_limits<double>::max)(), "NONNEGATIVE");

    /// @brief Requires the value to be strictly greater than zero.
    const range_t positive_number((std::numeric_limits<double>::min)(), (std::numeric_limits<double>::max)(),
                                  "POSITIVE");

    namespace detail
    {

        /// @brief Reports whether multiplying two signed values would overflow.
        ///
        /// @param a The left operand.
        /// @param b The right operand.
        /// @return `true` if the product would not be representable.
        template <typename T>
            requires std::is_signed_v<T>
        auto overflow_check(const T &a, const T &b) -> bool
        {
            if ((a > 0) == (b > 0))
            {
                return ((std::numeric_limits<T>::max)() / (std::abs)(a) < (std::abs)(b));
            }
            return ((std::numeric_limits<T>::min)() / (std::abs)(a) > -(std::abs)(b));
        }

        /// @brief Reports whether multiplying two unsigned values would overflow.
        ///
        /// @param a The left operand.
        /// @param b The right operand.
        /// @return `true` if the product would not be representable.
        template <typename T>
            requires(!std::is_signed_v<T>)
        auto overflow_check(const T &a, const T &b) -> bool
        {
            return ((std::numeric_limits<T>::max)() / a < b);
        }

        /// @brief Multiplies in place, refusing to overflow.
        ///
        /// @param[in,out] a The value to multiply; left untouched on failure.
        /// @param[in] b The multiplier.
        /// @return `true` if the multiplication was performed.
        template <typename T>
            requires std::is_integral_v<T>
        auto checked_multiply(T &a, T b) -> bool
        {
            if (a == 0 || b == 0 || a == 1 || b == 1)
            {
                a *= b;
                return true;
            }
            if (a == (std::numeric_limits<T>::min)() || b == (std::numeric_limits<T>::min)())
            {
                return false;
            }
            if (overflow_check(a, b))
            {
                return false;
            }
            a *= b;
            return true;
        }

        /// @brief Multiplies in place, refusing to produce an infinity.
        ///
        /// @param[in,out] a The value to multiply; left untouched on failure.
        /// @param[in] b The multiplier.
        /// @return `true` if the multiplication was performed.
        template <typename T>
            requires std::is_floating_point_v<T>
        auto checked_multiply(T &a, T b) -> bool
        {
            const T c = a * b;
            if (std::isinf(c) && !std::isinf(a) && !std::isinf(b))
            {
                return false;
            }
            a = c;
            return true;
        }

        /// @brief A command line split into the program and everything after it.
        struct program_name_t
        {
                /// @brief The program name, with any surrounding quotes removed.
                std::string name;

                /// @brief The remainder of the command line.
                std::string arguments;
        };

        /// @brief Separates the program name from the rest of a command line.
        ///
        /// The program name may contain spaces, so candidate prefixes are tested
        /// against the filesystem until one names a file. Quoted program names are
        /// unquoted, and escaped quotes within them are resolved.
        ///
        /// @param commandline The full command line.
        /// @return The program name and the remaining arguments.
        auto split_program_name(std::string commandline) -> program_name_t
        {
            program_name_t vals;
            trim(commandline);
            auto esp = commandline.find_first_of(' ', 1);

            while (check_path(commandline.substr(0, esp)) != path_type_t::file)
            {
                esp = commandline.find_first_of(' ', esp + 1);
                if (esp == std::string::npos)
                {
                    if (commandline[0] == '"' || commandline[0] == '\'' || commandline[0] == '`')
                    {
                        bool embedded_quote = false;
                        const auto key_char = commandline[0];
                        auto end = commandline.find_first_of(key_char, 1);
                        while ((end != std::string::npos) && (commandline[end - 1] == '\\'))
                        {
                            end = commandline.find_first_of(key_char, end + 1);
                            embedded_quote = true;
                        }
                        if (end != std::string::npos)
                        {
                            vals.name = commandline.substr(1, end - 1);
                            esp = end + 1;
                            if (embedded_quote)
                            {
                                vals.name = find_and_replace(vals.name, std::string("\\") + key_char,
                                                             std::string(1, key_char));
                            }
                        }
                        else
                        {
                            esp = commandline.find_first_of(' ', 1);
                        }
                    }
                    else
                    {
                        esp = commandline.find_first_of(' ', 1);
                    }
                    break;
                }
            }

            if (vals.name.empty())
            {
                vals.name = commandline.substr(0, esp);
                rtrim(vals.name);
            }
            vals.arguments = (esp < commandline.length() - 1) ? commandline.substr(esp + 1) : std::string {};
            ltrim(vals.arguments);
            return vals;
        }

    } // namespace detail

} // namespace cli
