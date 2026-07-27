/// @file
/// @brief The exception hierarchy thrown by the library.
///
/// Everything derives from @ref cli::error_t, which derives from
/// `std::runtime_error`. The hierarchy splits into two branches:
///
/// - @ref cli::construction_error_t and its descendants report programmer
///   mistakes made while building the parser, and are thrown from `add_option`
///   and friends.
/// - @ref cli::parse_error_t and its descendants report problems with the
///   command line itself, and are thrown from `parse`.
///
/// Catch @ref cli::parse_error_t in `main` and hand it to `app_t::exit`.
///
/// Each type carries a short name, reachable through @ref cli::error_t::get_name,
/// and a process exit code, reachable through @ref cli::error_t::get_exit_code.

export module cli11:error;

import std;
import :string_tools;

export namespace cli
{

    /// @brief Process exit codes associated with each error type.
    enum class exit_codes_t : int
    {
        success = 0,                  ///< No error.
        incorrect_construction = 100, ///< The parser was built incorrectly.
        bad_name_string,              ///< An option or subcommand name was malformed.
        option_already_added,         ///< A name collided with an existing option.
        file_error,                   ///< A file could not be read.
        conversion_error,             ///< A value could not be converted to its target type.
        validation_error,             ///< A value failed a validator.
        required_error,               ///< A required option or subcommand was absent.
        requires_error,               ///< An option was used without one it depends on.
        excludes_error,               ///< Two mutually exclusive options were used together.
        extras_error,                 ///< Unexpected arguments remained after parsing.
        config_error,                 ///< A configuration file could not be applied.
        invalid_error,                ///< The parser reached an invalid state.
        horrible_error,               ///< An internal invariant was violated; please report it.
        option_not_found,             ///< A lookup by name found no such option.
        argument_mismatch,            ///< An option received the wrong number of arguments.
        base_class = 127              ///< Default for an otherwise unclassified error.
    };

    /// @brief Base of every exception the library throws.
    class error_t : public std::runtime_error
    {
            /// @brief The process exit code associated with this error.
            int actual_exit_code;

            /// @brief The short name of this error type.
            std::string error_name {"error"};

        public:
            /// @brief Returns the process exit code associated with this error.
            ///
            /// @return The exit code.
            [[nodiscard]] auto get_exit_code() const -> int
            {
                return actual_exit_code;
            }

            /// @brief Returns the short name of this error type.
            ///
            /// @return The error name, for example `"validation_error"`.
            [[nodiscard]] auto get_name() const -> std::string
            {
                return error_name;
            }

            /// @brief Constructs an error with an explicit numeric exit code.
            ///
            /// @param name Short name of the error type.
            /// @param msg Human-readable description.
            /// @param exit_code Process exit code to report.
            error_t(std::string name, std::string msg, int exit_code = static_cast<int>(exit_codes_t::base_class))
                : runtime_error(std::move(msg)), actual_exit_code(exit_code), error_name(std::move(name))
            {
            }

            /// @brief Constructs an error with an exit code drawn from @ref exit_codes_t.
            ///
            /// @param name Short name of the error type.
            /// @param msg Human-readable description.
            /// @param exit_code Process exit code to report.
            error_t(std::string name, std::string msg, exit_codes_t exit_code)
                : error_t(std::move(name), std::move(msg), static_cast<int>(exit_code))
            {
            }
    };

    namespace detail
    {

        /// @brief Supplies the constructor set every error type shares.
        ///
        /// Each error type in the hierarchy needs the same four constructors: a
        /// protected pair taking an explicit type name, so that further derivation
        /// works, and a public pair that fills the name in automatically. This
        /// mixin generates all four, reading the name from
        /// `derived_t::error_type_name`.
        ///
        /// @tparam derived_t The error type being defined; must declare a
        /// `static constexpr std::string_view error_type_name`.
        /// @tparam base_t The error type to derive from.
        template <typename derived_t, typename base_t> class error_mixin_t : public base_t
        {
            protected:
                /// @brief Forwards an explicit type name, for further derivation.
                ///
                /// @param ename Short name of the concrete error type.
                /// @param msg Human-readable description.
                /// @param exit_code Process exit code to report.
                error_mixin_t(std::string ename, std::string msg, int exit_code)
                    : base_t(std::move(ename), std::move(msg), exit_code)
                {
                }

                /// @brief Forwards an explicit type name, for further derivation.
                ///
                /// @param ename Short name of the concrete error type.
                /// @param msg Human-readable description.
                /// @param exit_code Process exit code to report.
                error_mixin_t(std::string ename, std::string msg, exit_codes_t exit_code)
                    : base_t(std::move(ename), std::move(msg), exit_code)
                {
                }

            public:
                /// @brief Constructs the error, naming it from @p derived_t.
                ///
                /// @param msg Human-readable description.
                /// @param exit_code Process exit code to report.
                error_mixin_t(std::string msg, exit_codes_t exit_code)
                    : base_t(std::string {derived_t::error_type_name}, std::move(msg), exit_code)
                {
                }

                /// @brief Constructs the error, naming it from @p derived_t.
                ///
                /// @param msg Human-readable description.
                /// @param exit_code Process exit code to report.
                error_mixin_t(std::string msg, int exit_code)
                    : base_t(std::string {derived_t::error_type_name}, std::move(msg), exit_code)
                {
                }
        };

    } // namespace detail

    /// @brief Base of the errors reporting a mistake in how the parser was built.
    class construction_error_t : public detail::error_mixin_t<construction_error_t, error_t>
    {
            using mixin_t = detail::error_mixin_t<construction_error_t, error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "construction_error";

            using mixin_t::mixin_t;
    };

    /// @brief An option was configured in a way that cannot work.
    class incorrect_construction_t : public detail::error_mixin_t<incorrect_construction_t, construction_error_t>
    {
            using mixin_t = detail::error_mixin_t<incorrect_construction_t, construction_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "incorrect_construction";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit incorrect_construction_t(std::string msg)
                : mixin_t(std::move(msg), exit_codes_t::incorrect_construction)
            {
            }

            /// @brief A flag was declared as a positional.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto positional_flag(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(name + ": Flags cannot be positional");
            }

            /// @brief An option was told to expect zero values.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto set_0_opt(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(name + ": Cannot set 0 expected, use a flag instead");
            }

            /// @brief A flag was given an expected value count.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto set_flag(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(name + ": Cannot set an expected number for flags");
            }

            /// @brief A non-vector option was given a variable value count.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto change_not_vector(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(name + ": You can only change the expected arguments for vectors");
            }

            /// @brief The expected count was changed after the multi-option policy.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto after_multi_opt(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(
                    name + ": You can't change expected arguments after you've changed the multi option policy!");
            }

            /// @brief A named option does not exist.
            ///
            /// @param name The name that was looked up.
            /// @return The constructed error.
            [[nodiscard]] static auto missing_option(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t("Option " + name + " is not defined");
            }

            /// @brief A multi-option policy was applied where it cannot work.
            ///
            /// @param name The offending option name.
            /// @return The constructed error.
            [[nodiscard]] static auto multi_option_policy(const std::string &name) -> incorrect_construction_t
            {
                return incorrect_construction_t(name +
                                                ": multi_option_policy only works for flags and exact value options");
            }
    };

    /// @brief An option, subcommand, or positional name was malformed.
    class bad_name_string_t : public detail::error_mixin_t<bad_name_string_t, construction_error_t>
    {
            using mixin_t = detail::error_mixin_t<bad_name_string_t, construction_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "bad_name_string";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit bad_name_string_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::bad_name_string)
            {
            }

            /// @brief A single-character name was not valid.
            ///
            /// @param name The offending name.
            /// @return The constructed error.
            [[nodiscard]] static auto one_char_name(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Invalid one char name: " + name);
            }

            /// @brief A long name was given only one leading dash.
            ///
            /// @param name The offending name.
            /// @return The constructed error.
            [[nodiscard]] static auto missing_dash(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Long names strings require 2 dashes " + name);
            }

            /// @brief A long name contained invalid characters.
            ///
            /// @param name The offending name.
            /// @return The constructed error.
            [[nodiscard]] static auto bad_long_name(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Bad long name: " + name);
            }

            /// @brief A positional name contained invalid characters.
            ///
            /// @param name The offending name.
            /// @return The constructed error.
            [[nodiscard]] static auto bad_positional_name(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Invalid positional Name: " + name);
            }

            /// @brief A reserved name was used.
            ///
            /// @param name The offending name.
            /// @return The constructed error.
            [[nodiscard]] static auto reserved_name(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Names '-','--','++' are reserved and not allowed as option names " + name);
            }

            /// @brief More than one positional name was supplied.
            ///
            /// @param name The surplus name.
            /// @return The constructed error.
            [[nodiscard]] static auto multi_positional_names(const std::string &name) -> bad_name_string_t
            {
                return bad_name_string_t("Only one positional name allowed, remove: " + name);
            }
    };

    /// @brief A name collided with one already registered.
    class option_already_added_t : public detail::error_mixin_t<option_already_added_t, construction_error_t>
    {
            using mixin_t = detail::error_mixin_t<option_already_added_t, construction_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "option_already_added";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for a duplicated name.
            ///
            /// @param name The name that was already registered.
            explicit option_already_added_t(const std::string &name)
                : mixin_t(name + " is already added", exit_codes_t::option_already_added)
            {
            }

            /// @brief A dependency was declared twice.
            ///
            /// @param name The dependent option.
            /// @param other The option it depends on.
            /// @return The constructed error.
            ///
            /// @note Named `requires_` because `requires` is a keyword.
            [[nodiscard]] static auto requires_(const std::string &name, const std::string &other)
                -> option_already_added_t
            {
                return {name + " requires " + other, exit_codes_t::option_already_added};
            }

            /// @brief An exclusion was declared twice.
            ///
            /// @param name The excluding option.
            /// @param other The excluded option.
            /// @return The constructed error.
            [[nodiscard]] static auto excludes(const std::string &name, const std::string &other)
                -> option_already_added_t
            {
                return {name + " excludes " + other, exit_codes_t::option_already_added};
            }
    };

    /// @brief Base of the errors reporting a problem with the command line.
    ///
    /// This is the type to catch in `main`.
    class parse_error_t : public detail::error_mixin_t<parse_error_t, error_t>
    {
            using mixin_t = detail::error_mixin_t<parse_error_t, error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "parse_error";

            using mixin_t::mixin_t;
    };

    /// @brief Parsing finished early but successfully; the program should exit cleanly.
    class success_t : public detail::error_mixin_t<success_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<success_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "success";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default message.
            success_t() : mixin_t("Successfully completed, should be caught and quit", exit_codes_t::success)
            {
            }
    };

    /// @brief The help flag was given; print help and exit.
    class call_for_help_t : public detail::error_mixin_t<call_for_help_t, success_t>
    {
            using mixin_t = detail::error_mixin_t<call_for_help_t, success_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "call_for_help";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default message.
            call_for_help_t()
                : mixin_t("This should be caught in your main function, see examples", exit_codes_t::success)
            {
            }
    };

    /// @brief The expanded help flag was given; print full help and exit.
    class call_for_all_help_t : public detail::error_mixin_t<call_for_all_help_t, success_t>
    {
            using mixin_t = detail::error_mixin_t<call_for_all_help_t, success_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "call_for_all_help";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default message.
            call_for_all_help_t()
                : mixin_t("This should be caught in your main function, see examples", exit_codes_t::success)
            {
            }
    };

    /// @brief The version flag was given; print the version and exit.
    class call_for_version_t : public detail::error_mixin_t<call_for_version_t, success_t>
    {
            using mixin_t = detail::error_mixin_t<call_for_version_t, success_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "call_for_version";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default message.
            call_for_version_t()
                : mixin_t("This should be caught in your main function, see examples", exit_codes_t::success)
            {
            }
    };

    /// @brief The program requested a non-zero exit without a parsing problem.
    class runtime_error_t : public detail::error_mixin_t<runtime_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<runtime_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "runtime_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with a numeric exit code.
            ///
            /// @param exit_code Process exit code to report.
            explicit runtime_error_t(int exit_code = 1) : mixin_t("Runtime error", exit_code)
            {
            }
    };

    /// @brief A file could not be opened or read.
    class file_error_t : public detail::error_mixin_t<file_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<file_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "file_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit file_error_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::file_error)
            {
            }

            /// @brief A file was absent or unreadable.
            ///
            /// @param name The path that could not be read.
            /// @return The constructed error.
            [[nodiscard]] static auto missing(const std::string &name) -> file_error_t
            {
                return file_error_t(name + " was not readable (missing?)");
            }
    };

    /// @brief A value could not be converted to the option's target type.
    class conversion_error_t : public detail::error_mixin_t<conversion_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<conversion_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "conversion_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit conversion_error_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::conversion_error)
            {
            }

            /// @brief A value was rejected for a named option.
            ///
            /// @param member The offending value.
            /// @param name The option that rejected it.
            conversion_error_t(const std::string &member, const std::string &name)
                : conversion_error_t("The value " + member + " is not an allowed value for " + name)
            {
            }

            /// @brief A set of values could not be converted.
            ///
            /// @param name The option being filled.
            /// @param results The values that could not be converted.
            conversion_error_t(const std::string &name, const std::vector<std::string> &results)
                : conversion_error_t("Could not convert: " + name + " = " + detail::join(results))
            {
            }

            /// @brief A flag was given more than one value.
            ///
            /// @param name The offending flag.
            /// @return The constructed error.
            [[nodiscard]] static auto too_many_inputs_flag(const std::string &name) -> conversion_error_t
            {
                return conversion_error_t(name + ": too many inputs for a flag");
            }

            /// @brief A boolean value was neither true, false, nor numeric.
            ///
            /// @param name The offending option.
            /// @return The constructed error.
            [[nodiscard]] static auto true_false(const std::string &name) -> conversion_error_t
            {
                return conversion_error_t(name + ": Should be true/false or a number");
            }
    };

    /// @brief A value failed one of the option's validators.
    class validation_error_t : public detail::error_mixin_t<validation_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<validation_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "validation_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit validation_error_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::validation_error)
            {
            }

            /// @brief Constructs the error, prefixing the message with the option name.
            ///
            /// @param name The option that failed validation.
            /// @param msg Human-readable description.
            validation_error_t(const std::string &name, const std::string &msg) : validation_error_t(name + ": " + msg)
            {
            }
    };

    /// @brief A required option or subcommand was not supplied.
    class required_error_t : public detail::error_mixin_t<required_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<required_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "required_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for a missing named entity.
            ///
            /// @param name The entity that was required.
            explicit required_error_t(const std::string &name)
                : mixin_t(name + " is required", exit_codes_t::required_error)
            {
            }

            /// @brief Too few subcommands were supplied.
            ///
            /// @param min_subcom The minimum number required.
            /// @return The constructed error.
            [[nodiscard]] static auto subcommand(std::size_t min_subcom) -> required_error_t
            {
                if (min_subcom == 1)
                {
                    return required_error_t("A subcommand");
                }
                return {"Requires at least " + std::to_string(min_subcom) + " subcommands",
                        exit_codes_t::required_error};
            }

            /// @brief The number of options used fell outside the permitted range.
            ///
            /// @param min_option Minimum number of options required.
            /// @param max_option Maximum number of options permitted.
            /// @param used How many were actually supplied.
            /// @param option_list The options the requirement applies to.
            /// @return The constructed error.
            [[nodiscard]] static auto option(std::size_t min_option,
                                             std::size_t max_option,
                                             std::size_t used,
                                             const std::string &option_list) -> required_error_t
            {
                if ((min_option == 1) && (max_option == 1) && (used == 0))
                {
                    return required_error_t("Exactly 1 option from [" + option_list + "]");
                }
                if ((min_option == 1) && (max_option == 1) && (used > 1))
                {
                    return {"Exactly 1 option from [" + option_list + "] is required but " + std::to_string(used) +
                                " were given",
                            exit_codes_t::required_error};
                }
                if ((min_option == 1) && (used == 0))
                {
                    return required_error_t("At least 1 option from [" + option_list + "]");
                }
                if (used < min_option)
                {
                    return {"Requires at least " + std::to_string(min_option) + " options used but only " +
                                std::to_string(used) + " were given from [" + option_list + "]",
                            exit_codes_t::required_error};
                }
                if (max_option == 1)
                {
                    return {"Requires at most 1 options be given from [" + option_list + "]",
                            exit_codes_t::required_error};
                }
                return {"Requires at most " + std::to_string(max_option) + " options be used but " +
                            std::to_string(used) + " were given from [" + option_list + "]",
                        exit_codes_t::required_error};
            }
    };

    /// @brief An option received the wrong number of values.
    class argument_mismatch_t : public detail::error_mixin_t<argument_mismatch_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<argument_mismatch_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "argument_mismatch";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit argument_mismatch_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::argument_mismatch)
            {
            }

            /// @brief An option received a count other than the one it expects.
            ///
            /// @param name The offending option.
            /// @param expected Expected count; negative means "at least this many".
            /// @param received How many values arrived.
            argument_mismatch_t(const std::string &name, int expected, std::size_t received)
                : mixin_t(expected > 0 ? ("Expected exactly " + std::to_string(expected) + " arguments to " + name +
                                          ", got " + std::to_string(received))
                                       : ("Expected at least " + std::to_string(-expected) + " arguments to " + name +
                                          ", got " + std::to_string(received)),
                          exit_codes_t::argument_mismatch)
            {
            }

            /// @brief Fewer values arrived than the minimum.
            ///
            /// @param name The offending option.
            /// @param num The minimum required.
            /// @param received How many values arrived.
            /// @return The constructed error.
            [[nodiscard]] static auto at_least(const std::string &name, int num, std::size_t received)
                -> argument_mismatch_t
            {
                return argument_mismatch_t(name + ": At least " + std::to_string(num) + " required but received " +
                                           std::to_string(received));
            }

            /// @brief More values arrived than the maximum.
            ///
            /// @param name The offending option.
            /// @param num The maximum permitted.
            /// @param received How many values arrived.
            /// @return The constructed error.
            [[nodiscard]] static auto at_most(const std::string &name, int num, std::size_t received)
                -> argument_mismatch_t
            {
                return argument_mismatch_t(name + ": At most " + std::to_string(num) + " required but received " +
                                           std::to_string(received));
            }

            /// @brief Too few values arrived to fill one element of a typed option.
            ///
            /// @param name The offending option.
            /// @param num The number required.
            /// @param type Name of the element type.
            /// @return The constructed error.
            [[nodiscard]] static auto typed_at_least(const std::string &name, int num, const std::string &type)
                -> argument_mismatch_t
            {
                return argument_mismatch_t(name + ": " + std::to_string(num) + " required " + type + " missing");
            }

            /// @brief A flag override was supplied where none is allowed.
            ///
            /// @param name The offending flag.
            /// @return The constructed error.
            [[nodiscard]] static auto flag_override(const std::string &name) -> argument_mismatch_t
            {
                return argument_mismatch_t(name + " was given a disallowed flag override");
            }

            /// @brief A compound element was only partially supplied.
            ///
            /// @param name The offending option.
            /// @param num Values required per element.
            /// @param type Name of the element type.
            /// @return The constructed error.
            [[nodiscard]] static auto partial_type(const std::string &name, int num, const std::string &type)
                -> argument_mismatch_t
            {
                return argument_mismatch_t(name + ": " + type + " only partially specified: " + std::to_string(num) +
                                           " required for each element");
            }
    };

    /// @brief An option was used without another it depends on.
    class requires_error_t : public detail::error_mixin_t<requires_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<requires_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "requires_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for an unmet dependency.
            ///
            /// @param curname The option that was used.
            /// @param subname The option it needs.
            requires_error_t(const std::string &curname, const std::string &subname)
                : mixin_t(curname + " requires " + subname, exit_codes_t::requires_error)
            {
            }
    };

    /// @brief Two mutually exclusive options were used together.
    class excludes_error_t : public detail::error_mixin_t<excludes_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<excludes_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "excludes_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for a violated exclusion.
            ///
            /// @param curname The option that was used.
            /// @param subname The option it excludes.
            excludes_error_t(const std::string &curname, const std::string &subname)
                : mixin_t(curname + " excludes " + subname, exit_codes_t::excludes_error)
            {
            }
    };

    /// @brief Arguments remained on the command line after parsing finished.
    class extras_error_t : public detail::error_mixin_t<extras_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<extras_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "extras_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error from the leftover arguments.
            ///
            /// @param args The arguments that were not consumed.
            explicit extras_error_t(const std::vector<std::string> &args)
                : mixin_t((args.size() > 1 ? "The following arguments were not expected: "
                                           : "The following argument was not expected: ") +
                              detail::join(args, " "),
                          exit_codes_t::extras_error)
            {
            }

            /// @brief Constructs the error from the leftover arguments of a subcommand.
            ///
            /// @param name The subcommand the arguments were left over from.
            /// @param args The arguments that were not consumed.
            extras_error_t(const std::string &name, const std::vector<std::string> &args)
                : mixin_t(name,
                          (args.size() > 1 ? "The following arguments were not expected: "
                                           : "The following argument was not expected: ") +
                              detail::join(args, " "),
                          exit_codes_t::extras_error)
            {
            }
    };

    /// @brief A configuration file could not be parsed or applied.
    class config_error_t : public detail::error_mixin_t<config_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<config_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "config_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit config_error_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::config_error)
            {
            }

            /// @brief A configuration entry could not be parsed.
            ///
            /// @param item The entry that failed.
            /// @return The constructed error.
            [[nodiscard]] static auto extras(const std::string &item) -> config_error_t
            {
                return config_error_t("INI was not able to parse " + item);
            }

            /// @brief A configuration entry named an option that may not be configured.
            ///
            /// @param item The offending entry.
            /// @return The constructed error.
            [[nodiscard]] static auto not_configurable(const std::string &item) -> config_error_t
            {
                return config_error_t(item + ": This option is not allowed in a configuration file");
            }
    };

    /// @brief The parser reached a state it cannot resolve.
    class invalid_error_t : public detail::error_mixin_t<invalid_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<invalid_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "invalid_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for an unresolvable positional layout.
            ///
            /// @param name The offending option.
            explicit invalid_error_t(const std::string &name)
                : mixin_t(name + ": Too many positional arguments with unlimited expected args",
                          exit_codes_t::invalid_error)
            {
            }
    };

    /// @brief An internal invariant was violated.
    ///
    /// Reaching this indicates a bug in the library rather than in the calling code.
    class horrible_error_t : public detail::error_mixin_t<horrible_error_t, parse_error_t>
    {
            using mixin_t = detail::error_mixin_t<horrible_error_t, parse_error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "horrible_error";

            using mixin_t::mixin_t;

            /// @brief Constructs the error with the default exit code.
            ///
            /// @param msg Human-readable description.
            explicit horrible_error_t(std::string msg) : mixin_t(std::move(msg), exit_codes_t::horrible_error)
            {
            }
    };

    /// @brief A lookup by name found no matching option.
    ///
    /// This derives from @ref error_t rather than @ref parse_error_t: it reports a
    /// bad query from the calling code, not a bad command line.
    class option_not_found_t : public detail::error_mixin_t<option_not_found_t, error_t>
    {
            using mixin_t = detail::error_mixin_t<option_not_found_t, error_t>;

        public:
            /// @brief Short name reported by @ref error_t::get_name.
            static constexpr std::string_view error_type_name = "option_not_found";

            using mixin_t::mixin_t;

            /// @brief Constructs the error for a failed lookup.
            ///
            /// @param name The name that was looked up.
            explicit option_not_found_t(const std::string &name)
                : mixin_t(name + " not found", exit_codes_t::option_not_found)
            {
            }
    };

} // namespace cli
