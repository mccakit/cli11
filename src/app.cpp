module;
#include <cerrno>
export module cli11:app;

import std;
import :string_tools;
import :error;
import :option;
import :split;
import :type_tools;
import :validators;
import :encoding;
import :config_fwd;
import :formatter_fwd;

export namespace CLI
{

    namespace detail
    {
        enum class Classifier : std::uint8_t
        {
            NONE,
            POSITIONAL_MARK,
            SHORT,
            LONG,
            WINDOWS_STYLE,
            SUBCOMMAND,
            SUBCOMMAND_TERMINATOR
        };
        struct AppFriend;
    } // namespace detail

    namespace FailureMessage
    {
        /// Printout a clean, simple message on error (the default in CLI11 1.5+)
        std::string simple(const App *app, const Error &e);

        /// Printout the full help string on error (if this fn is set, the old default for CLI11)
        std::string help(const App *app, const Error &e);
    } // namespace FailureMessage

    /// enumeration of modes of how to deal with command line extras
    enum class ExtrasMode : std::uint8_t
    {
        Error = 0,
        ErrorImmediately,
        Ignore,
        AssumeSingleArgument,
        AssumeMultipleArguments,
        Capture
    };

    /// enumeration of modes of how to deal with extras in config files
    enum class ConfigExtrasMode : std::uint8_t
    {
        Error = 0,
        Ignore,
        IgnoreAll,
        Capture
    };

    /// @brief  enumeration of modes of how to deal with extras in config files
    enum class config_extras_mode : std::uint8_t
    {
        error = 0,
        ignore,
        ignore_all,
        capture
    };

    /// @brief  enumeration of prefix command modes, separator requires that the first extra argument be a "--", other
    /// unrecognized arguments will cause an error. on allows the first extra to trigger prefix mode regardless of other
    /// recognized options
    enum class PrefixCommandMode : std::uint8_t
    {
        Off = 0,
        SeparatorOnly = 1,
        On = 2
    };

    class App;

    using App_p = std::shared_ptr<App>;

    namespace detail
    {
        /// helper functions for adding in appropriate flag modifiers for add_flag

        template <typename T,
                  enable_if_t<!std::is_integral<T>::value || (sizeof(T) <= 1U), detail::enabler> = detail::dummy>
        Option *default_flag_modifiers(Option *opt)
        {
            return opt->always_capture_default();
        }

        /// summing modifiers
        template <typename T,
                  enable_if_t<std::is_integral<T>::value && (sizeof(T) > 1U), detail::enabler> = detail::dummy>
        Option *default_flag_modifiers(Option *opt)
        {
            return opt->multi_option_policy(MultiOptionPolicy::Sum)->default_str("0")->force_callback();
        }

    } // namespace detail

    class Option_group;
    /// Creates a command line program, with very few defaults.
    /** To use, create a new `Program()` instance with `argc`, `argv`, and a help description. The templated
     *  add_option methods make it easy to prepare options. Remember to call `.start` before starting your
     * program, so that the options can be evaluated and the help option doesn't accidentally run your program. */
    class App
    {
            friend Option;
            friend detail::AppFriend;

        protected:
            // This library follows the Google style guide for member names ending in underscores

            /// @name Basics
            ///@{

            /// Subcommand name or program name (from parser if name is empty)
            std::string name_ {};

            /// Description of the current program/subcommand
            std::string description_ {};

            /// If true, allow extra arguments (ie, don't throw an error). INHERITABLE
            ExtrasMode allow_extras_ {ExtrasMode::Error};

            /// If ignore, allow extra arguments in the ini file (ie, don't throw an error). INHERITABLE
            /// if error, error on an extra argument, and if capture feed it to the app
            ConfigExtrasMode allow_config_extras_ {ConfigExtrasMode::Ignore};

            ///  If true, cease processing on an unrecognized option (implies allow_extras) INHERITABLE
            PrefixCommandMode prefix_command_ {PrefixCommandMode::Off};

            /// If set to true the name was automatically generated from the command line vs a user set name
            bool has_automatic_name_ {false};

            /// If set to true the subcommand is required to be processed and used, ignored for main app
            bool required_ {false};

            /// If set to true the subcommand is disabled and cannot be used, ignored for main app
            bool disabled_ {false};

            /// Flag indicating that the pre_parse_callback has been triggered
            bool pre_parse_called_ {false};

            /// Flag indicating that the callback for the subcommand should be executed immediately on parse completion
            /// which is before help or ini files are processed. INHERITABLE
            bool immediate_callback_ {false};

            /// This is a function that runs prior to the start of parsing
            std::function<void(std::size_t)> pre_parse_callback_ {};

            /// This is a function that runs when parsing has finished.
            std::function<void()> parse_complete_callback_ {};

            /// This is a function that runs when all processing has completed
            std::function<void()> final_callback_ {};

            ///@}
            /// @name Options
            ///@{

            /// The default values for options, customizable and changeable INHERITABLE
            OptionDefaults option_defaults_ {};

            /// The list of options, stored locally
            std::vector<Option_p> options_ {};

            ///@}
            /// @name Help
            ///@{

            /// Usage to put after program/subcommand description in the help output INHERITABLE
            std::string usage_ {};

            /// This is a function that generates a usage to put after program/subcommand description in help output
            std::function<std::string()> usage_callback_ {};

            /// Footer to put after all options in the help output INHERITABLE
            std::string footer_ {};

            /// This is a function that generates a footer to put after all other options in help output
            std::function<std::string()> footer_callback_ {};

            /// A pointer to the help flag if there is one INHERITABLE
            Option *help_ptr_ {nullptr};

            /// A pointer to the help all flag if there is one INHERITABLE
            Option *help_all_ptr_ {nullptr};

            /// A pointer to a version flag if there is one
            Option *version_ptr_ {nullptr};

            /// This is the formatter for help printing. Default provided. INHERITABLE (same pointer)
            std::shared_ptr<FormatterBase> formatter_ {new Formatter()};

            /// The error message printing function INHERITABLE
            std::function<std::string(const App *, const Error &e)> failure_message_ {FailureMessage::simple};

            ///@}
            /// @name Parsing
            ///@{

            using missing_t = std::vector<std::pair<detail::Classifier, std::string>>;

            /// Pair of classifier, string for missing options. (extra detail is removed on returning from parse)
            ///
            /// This is faster and cleaner than storing just a list of strings and reparsing. This may contain the --
            /// separator.
            missing_t missing_ {};

            /// This is a list of pointers to options with the original parse order
            std::vector<Option *> parse_order_ {};

            /// This is a list of the subcommands collected, in order
            std::vector<App *> parsed_subcommands_ {};

            /// this is a list of subcommands that are exclusionary to this one
            std::set<App *> exclude_subcommands_ {};

            /// This is a list of options which are exclusionary to this App, if the options were used this subcommand
            /// should not be
            std::set<Option *> exclude_options_ {};

            /// this is a list of subcommands or option groups that are required by this one, the list is not mutual,
            /// the listed subcommands do not require this one
            std::set<App *> need_subcommands_ {};

            /// This is a list of options which are required by this app, the list is not mutual, listed options do not
            /// need the subcommand not be
            std::set<Option *> need_options_ {};

            ///@}
            /// @name Subcommands
            ///@{

            /// Storage for subcommand list
            std::vector<App_p> subcommands_ {};

            /// If true, the program name is not case-sensitive INHERITABLE
            bool ignore_case_ {false};

            /// If true, the program should ignore underscores INHERITABLE
            bool ignore_underscore_ {false};

            /// Allow options or other arguments to fallthrough, so that parent commands can collect options after
            /// subcommand. INHERITABLE
            bool fallthrough_ {false};

            /// Allow subcommands to fallthrough, so that parent commands can trigger other subcommands after
            /// subcommand.
            bool subcommand_fallthrough_ {true};

            /// Allow '/' for options for Windows like options. Defaults to true on Windows, false otherwise.
            /// INHERITABLE
            bool allow_windows_style_options_ {
#ifdef _WIN32
                true
#else
                false
#endif
            };
            /// specify that positional arguments come at the end of the argument sequence not inheritable
            bool positionals_at_end_ {false};

            enum class startup_mode : std::uint8_t
            {
                stable,
                enabled,
                disabled
            };
            /// specify the startup mode for the app
            /// stable=no change, enabled= startup enabled, disabled=startup disabled
            startup_mode default_startup {startup_mode::stable};

            /// if set to true the subcommand can be triggered via configuration files INHERITABLE
            bool configurable_ {false};

            /// If set to true positional options are validated before assigning INHERITABLE
            bool validate_positionals_ {false};

            /// If set to true optional vector arguments are validated before assigning INHERITABLE
            bool validate_optional_arguments_ {false};

            /// indicator that the subcommand is silent and won't show up in subcommands list
            /// This is potentially useful as a modifier subcommand
            bool silent_ {false};

            /// indicator that the subcommand should allow non-standard option arguments, such as -single_dash_flag
            bool allow_non_standard_options_ {false};

            /// indicator to allow subcommands to match with prefix matching
            bool allow_prefix_matching_ {false};

            /// Counts the number of times this command/subcommand was parsed
            std::uint32_t parsed_ {0U};

            /// Minimum required subcommands (not inheritable!)
            std::size_t require_subcommand_min_ {0};

            /// Max number of subcommands allowed (parsing stops after this number). 0 is unlimited INHERITABLE
            std::size_t require_subcommand_max_ {0};

            /// Minimum required options (not inheritable!)
            std::size_t require_option_min_ {0};

            /// Max number of options allowed. 0 is unlimited (not inheritable)
            std::size_t require_option_max_ {0};

            /// A pointer to the parent if this is a subcommand
            App *parent_ {nullptr};

            /// The group membership INHERITABLE
            std::string group_ {"SUBCOMMANDS"};

            /// Alias names for the subcommand
            std::vector<std::string> aliases_ {};

            ///@}
            /// @name Config
            ///@{

            /// Pointer to the config option
            Option *config_ptr_ {nullptr};

            /// This is the formatter for help printing. Default provided. INHERITABLE (same pointer)
            std::shared_ptr<Config> config_formatter_ {new ConfigTOML()};

            ///@}

#ifdef _WIN32
            /// When normalizing argv to UTF-8 on Windows, this is the storage for normalized args.
            std::vector<std::string> normalized_argv_ {};

            /// When normalizing argv to UTF-8 on Windows, this is the `char**` value returned to the user.
            std::vector<char *> normalized_argv_view_ {};
#endif

            /// Special private constructor for subcommand
            App(std::string app_description, std::string app_name, App *parent);

        public:
            /// @name Basic
            ///@{

            /// Create a new program. Pass in the same arguments as main(), along with a help string.
            explicit App(std::string app_description = "", std::string app_name = "")
                : App(app_description, app_name, nullptr)
            {
                set_help_flag("-h,--help", "Print this help message and exit");
            }

            App(const App &) = delete;
            App &operator=(const App &) = delete;

            /// virtual destructor
            virtual ~App() = default;

            /// Convert the contents of argv to UTF-8. Only does something on Windows, does nothing elsewhere.
            [[nodiscard]] char **ensure_utf8(char **argv);

            /// Set a callback for execution when all parsing and processing has completed
            ///
            /// Due to a bug in c++11,
            /// it is not possible to overload on std::function (fixed in c++14
            /// and backported to c++11 on newer compilers). Use capture by reference
            /// to get a pointer to App if needed.
            App *callback(std::function<void()> app_callback)
            {
                if (immediate_callback_)
                {
                    parse_complete_callback_ = std::move(app_callback);
                }
                else
                {
                    final_callback_ = std::move(app_callback);
                }
                return this;
            }

            /// Set a callback for execution when all parsing and processing has completed
            /// aliased as callback
            App *final_callback(std::function<void()> app_callback)
            {
                final_callback_ = std::move(app_callback);
                return this;
            }

            /// Set a callback to execute when parsing has completed for the app
            ///
            App *parse_complete_callback(std::function<void()> pc_callback)
            {
                parse_complete_callback_ = std::move(pc_callback);
                return this;
            }

            /// Set a callback to execute prior to parsing.
            ///
            App *preparse_callback(std::function<void(std::size_t)> pp_callback)
            {
                pre_parse_callback_ = std::move(pp_callback);
                return this;
            }

            /// Set a name for the app (empty will use parser to set the name)
            App *name(std::string app_name = "");

            /// Set an alias for the app
            App *alias(std::string app_name);

            /// Remove the error when extras are left over on the command line.
            App *allow_extras(bool allow = true)
            {
                allow_extras_ = allow ? ExtrasMode::Capture : ExtrasMode::Error;
                return this;
            }

            /// Remove the error when extras are left over on the command line.
            App *allow_extras(ExtrasMode allow)
            {
                allow_extras_ = allow;
                return this;
            }

            /// Remove the error when extras are left over on the command line.
            App *required(bool require = true)
            {
                required_ = require;
                return this;
            }

            /// Disable the subcommand or option group
            App *disabled(bool disable = true)
            {
                disabled_ = disable;
                return this;
            }

            /// silence the subcommand from showing up in the processed list
            App *silent(bool silence = true)
            {
                silent_ = silence;
                return this;
            }

            /// allow non standard option names
            App *allow_non_standard_option_names(bool allowed = true)
            {
                allow_non_standard_options_ = allowed;
                return this;
            }

            /// allow prefix matching for subcommands
            App *allow_subcommand_prefix_matching(bool allowed = true)
            {
                allow_prefix_matching_ = allowed;
                return this;
            }
            /// Set the subcommand to be disabled by default, so on clear(), at the start of each parse it is disabled
            App *disabled_by_default(bool disable = true)
            {
                if (disable)
                {
                    default_startup = startup_mode::disabled;
                }
                else
                {
                    default_startup =
                        (default_startup == startup_mode::enabled) ? startup_mode::enabled : startup_mode::stable;
                }
                return this;
            }

            /// Set the subcommand to be enabled by default, so on clear(), at the start of each parse it is enabled
            /// (not disabled)
            App *enabled_by_default(bool enable = true)
            {
                if (enable)
                {
                    default_startup = startup_mode::enabled;
                }
                else
                {
                    default_startup =
                        (default_startup == startup_mode::disabled) ? startup_mode::disabled : startup_mode::stable;
                }
                return this;
            }

            /// Set the subcommand callback to be executed immediately on subcommand completion
            App *immediate_callback(bool immediate = true);

            /// Set the subcommand to validate positional arguments before assigning
            App *validate_positionals(bool validate = true)
            {
                validate_positionals_ = validate;
                return this;
            }

            /// Set the subcommand to validate optional vector arguments before assigning
            App *validate_optional_arguments(bool validate = true)
            {
                validate_optional_arguments_ = validate;
                return this;
            }

            /// ignore extras in config files
            App *allow_config_extras(bool allow = true)
            {
                if (allow)
                {
                    allow_config_extras_ = ConfigExtrasMode::Capture;
                    allow_extras_ = ExtrasMode::Capture;
                }
                else
                {
                    allow_config_extras_ = ConfigExtrasMode::Error;
                }
                return this;
            }

            /// ignore extras in config files
            App *allow_config_extras(config_extras_mode mode)
            {
                allow_config_extras_ = static_cast<ConfigExtrasMode>(mode);
                return this;
            }

            /// ignore extras in config files
            App *allow_config_extras(ConfigExtrasMode mode)
            {
                allow_config_extras_ = mode;
                return this;
            }

            /// Enable or disable prefix command mode. If enabled, parsing stops at the
            /// first unrecognized option and all remaining arguments are stored in
            /// remaining args.
            App *prefix_command(bool is_prefix = true)
            {
                prefix_command_ = is_prefix ? PrefixCommandMode::On : PrefixCommandMode::Off;
                return this;
            }

            /// Set the prefix command mode directly.
            App *prefix_command(PrefixCommandMode mode)
            {
                prefix_command_ = mode;
                return this;
            }

            /// Ignore case. Subcommands inherit value.
            App *ignore_case(bool value = true);

            /// Allow windows style options, such as `/opt`. First matching short or long name used. Subcommands inherit
            /// value.
            App *allow_windows_style_options(bool value = true)
            {
                allow_windows_style_options_ = value;
                return this;
            }

            /// Specify that the positional arguments are only at the end of the sequence
            App *positionals_at_end(bool value = true)
            {
                positionals_at_end_ = value;
                return this;
            }

            /// Specify that the subcommand can be triggered by a config file
            App *configurable(bool value = true)
            {
                configurable_ = value;
                return this;
            }

            /// Ignore underscore. Subcommands inherit value.
            App *ignore_underscore(bool value = true);

            /// Set the help formatter
            App *formatter(std::shared_ptr<FormatterBase> fmt)
            {
                formatter_ = fmt;
                return this;
            }

            /// Set the help formatter
            App *formatter_fn(std::function<std::string(const App *, std::string, AppFormatMode)> fmt)
            {
                formatter_ = std::make_shared<FormatterLambda>(fmt);
                return this;
            }

            /// Set the config formatter
            App *config_formatter(std::shared_ptr<Config> fmt)
            {
                config_formatter_ = fmt;
                return this;
            }

            /// Check to see if this subcommand was parsed, true only if received on command line.
            [[nodiscard]] bool parsed() const
            {
                return parsed_ > 0;
            }

            /// Get the OptionDefault object, to set option defaults
            OptionDefaults *option_defaults()
            {
                return &option_defaults_;
            }

            ///@}
            /// @name Adding options
            ///@{

            /// Add an option, will automatically understand the type for common types.
            ///
            /// To use, create a variable with the expected type, and pass it in after the name.
            /// After start is called, you can use count to see if the value was passed, and
            /// the value will be initialized properly. Numbers, vectors, and strings are supported.
            ///
            /// ->required(), ->default, and the validators are options,
            /// The positional options take an optional number of arguments.
            ///
            /// For example,
            ///
            ///     std::string filename;
            ///     program.add_option("filename", filename, "description of filename");
            ///
            Option *add_option(std::string option_name,
                               callback_t option_callback,
                               std::string option_description = "",
                               bool defaulted = false,
                               std::function<std::string()> func = {});

            /// Add option for assigning to a variable
            template <typename AssignTo,
                      typename ConvertTo = AssignTo,
                      enable_if_t<!std::is_const<ConvertTo>::value, detail::enabler> = detail::dummy>
            Option *add_option(std::string option_name,
                               AssignTo &variable, ///< The variable to set
                               std::string option_description = "")
            {

                auto fun = [&variable](const CLI::results_t &res) { // comment for spacing
                    return detail::lexical_conversion<AssignTo, ConvertTo>(res, variable);
                };

                Option *opt = add_option(option_name, fun, option_description, false, [&variable]() {
                    return CLI::detail::checked_to_string<AssignTo, ConvertTo>(variable);
                });
                opt->type_name(detail::type_name<ConvertTo>());
                // these must be actual lvalues since (std::max) sometimes is defined in terms of references and
                // references to structs used in the evaluation can be temporary so that would cause issues.
                auto Tcount = detail::type_count<AssignTo>::value;
                auto XCcount = detail::type_count<ConvertTo>::value;
                opt->type_size(detail::type_count_min<ConvertTo>::value, (std::max)(Tcount, XCcount));
                opt->expected(detail::expected_count<ConvertTo>::value);
                opt->run_callback_for_default();
                return opt;
            }

            /// Add option for assigning to a variable
            template <typename AssignTo, enable_if_t<!std::is_const<AssignTo>::value, detail::enabler> = detail::dummy>
            Option *add_option_no_stream(std::string option_name,
                                         AssignTo &variable, ///< The variable to set
                                         std::string option_description = "")
            {

                auto fun = [&variable](const CLI::results_t &res) { // comment for spacing
                    return detail::lexical_conversion<AssignTo, AssignTo>(res, variable);
                };

                Option *opt = add_option(option_name, fun, option_description, false, []() { return std::string {}; });
                opt->type_name(detail::type_name<AssignTo>());
                opt->type_size(detail::type_count_min<AssignTo>::value, detail::type_count<AssignTo>::value);
                opt->expected(detail::expected_count<AssignTo>::value);
                opt->run_callback_for_default();
                return opt;
            }

            /// Add option for a callback of a specific type
            template <typename ArgType>
            Option *add_option_function(std::string option_name,
                                        const std::function<void(const ArgType &)> &func, ///< the callback to execute
                                        std::string option_description = "")
            {

                auto fun = [func](const CLI::results_t &res) {
                    ArgType variable;
                    bool result = detail::lexical_conversion<ArgType, ArgType>(res, variable);
                    if (result)
                    {
                        func(variable);
                    }
                    return result;
                };

                Option *opt = add_option(option_name, std::move(fun), option_description, false);
                opt->type_name(detail::type_name<ArgType>());
                opt->type_size(detail::type_count_min<ArgType>::value, detail::type_count<ArgType>::value);
                opt->expected(detail::expected_count<ArgType>::value);
                return opt;
            }

            /// Add option with no description or variable assignment
            Option *add_option(std::string option_name)
            {
                return add_option(option_name, CLI::callback_t {}, std::string {}, false);
            }

            /// Add option with description but with no variable assignment or callback
            template <typename T,
                      enable_if_t<std::is_const<T>::value && std::is_constructible<std::string, T>::value,
                                  detail::enabler> = detail::dummy>
            Option *add_option(std::string option_name, T &option_description)
            {
                return add_option(option_name, CLI::callback_t(), option_description, false);
            }

            /// Set a help flag, replace the existing one if present
            Option *set_help_flag(std::string flag_name = "", const std::string &help_description = "");

            /// Set a help all flag, replaced the existing one if present
            Option *set_help_all_flag(std::string help_name = "", const std::string &help_description = "");

            /// Set a version flag and version display string, replace the existing one if present
            Option *set_version_flag(std::string flag_name = "",
                                     const std::string &versionString = "",
                                     const std::string &version_help = "Display program version information and exit");

            /// Generate the version string through a callback function
            Option *set_version_flag(std::string flag_name,
                                     std::function<std::string()> vfunc,
                                     const std::string &version_help = "Display program version information and exit");

        private:
            /// Internal function for adding a flag
            Option *_add_flag_internal(std::string flag_name, CLI::callback_t fun, std::string flag_description);

        public:
            /// Add a flag with no description or variable assignment
            Option *add_flag(std::string flag_name)
            {
                return _add_flag_internal(flag_name, CLI::callback_t(), std::string {});
            }

            /// Add flag with description but with no variable assignment or callback
            /// takes a constant string or a rvalue reference to a string,  if a variable string is passed that variable
            /// will be assigned the results from the flag
            template <
                typename T,
                enable_if_t<(std::is_const<typename std::remove_reference<T>::type>::value ||
                             std::is_rvalue_reference<T &&>::value) &&
                                std::is_constructible<std::string, typename std::remove_reference<T>::type>::value,
                            detail::enabler> = detail::dummy>
            Option *add_flag(std::string flag_name, T &&flag_description)
            {
                return _add_flag_internal(flag_name, CLI::callback_t(), std::forward<T>(flag_description));
            }

            /// Other type version accepts all other types that are not vectors such as bool, enum, string or other
            /// classes that can be converted from a string
            template <typename T,
                      enable_if_t<!detail::is_mutable_container<T>::value && !std::is_const<T>::value &&
                                      !std::is_constructible<std::function<void(int)>, T>::value,
                                  detail::enabler> = detail::dummy>
            Option *add_flag(std::string flag_name,
                             T &flag_result, ///< A variable holding the flag result
                             std::string flag_description = "")
            {

                CLI::callback_t fun = [&flag_result](const CLI::results_t &res) {
                    using CLI::detail::lexical_cast;
                    return lexical_cast(res[0], flag_result);
                };
                auto *opt = _add_flag_internal(flag_name, std::move(fun), std::move(flag_description));
                return detail::default_flag_modifiers<T>(opt);
            }

            /// Vector version to capture multiple flags.
            template <typename T,
                      enable_if_t<!std::is_assignable<std::function<void(std::int64_t)> &, T>::value, detail::enabler> =
                          detail::dummy>
            Option *add_flag(std::string flag_name,
                             std::vector<T> &flag_results, ///< A vector of values with the flag results
                             std::string flag_description = "")
            {
                CLI::callback_t fun = [&flag_results](const CLI::results_t &res) {
                    bool retval = true;
                    for (const auto &elem : res)
                    {
                        using CLI::detail::lexical_cast;
                        flag_results.emplace_back();
                        retval &= lexical_cast(elem, flag_results.back());
                    }
                    return retval;
                };
                return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description))
                    ->multi_option_policy(MultiOptionPolicy::TakeAll)
                    ->run_callback_for_default();
            }

            /// Add option for callback that is triggered with a true flag and takes no arguments
            Option *add_flag_callback(std::string flag_name,
                                      std::function<void(void)> function, ///< A function to call, void(void)
                                      std::string flag_description = "");

            /// Add option for callback with an integer value
            Option *add_flag_function(std::string flag_name,
                                      std::function<void(std::int64_t)> function, ///< A function to call, void(int)
                                      std::string flag_description = "");

            /// Add option for callback (C++14 or better only)
            Option *add_flag(std::string flag_name,
                             std::function<void(std::int64_t)> function, ///< A function to call, void(std::int64_t)
                             std::string flag_description = "")
            {
                return add_flag_function(std::move(flag_name), std::move(function), std::move(flag_description));
            }

            /// Set a configuration ini file option, or clear it if no name passed
            Option *set_config(std::string option_name = "",
                               std::string default_filename = "",
                               const std::string &help_message = "Read an ini file",
                               bool config_required = false);

            /// Removes an option from the App. Takes an option pointer. Returns true if found and removed.
            bool remove_option(Option *opt);

            /// creates an option group as part of the given app
            template <typename T = Option_group>
            T *add_option_group(std::string group_name, std::string group_description = "")
            {
                if (!detail::valid_alias_name_string(group_name))
                {
                    throw IncorrectConstruction("option group names may not contain newlines or null characters");
                }
                auto option_group = std::make_shared<T>(std::move(group_description), group_name, this);
                option_group->fallthrough(false);
                auto *ptr = option_group.get();
                // move to App_p for overload resolution on older gcc versions
                App_p app_ptr = std::static_pointer_cast<App>(option_group);
                // don't inherit the footer in option groups and clear the help flag by default
                app_ptr->footer_ = "";
                app_ptr->set_help_flag();
                add_subcommand(std::move(app_ptr));
                return ptr;
            }

            ///@}
            /// @name Subcommands
            ///@{

            /// Add a subcommand. Inherits INHERITABLE and OptionDefaults, and help flag
            App *add_subcommand(std::string subcommand_name = "", std::string subcommand_description = "");

            /// Add a previously created app as a subcommand
            App *add_subcommand(CLI::App_p subcom);

            /// Removes a subcommand from the App. Takes a subcommand pointer. Returns true if found and removed.
            bool remove_subcommand(App *subcom);

            /// Check to see if a subcommand is part of this command (doesn't have to be in command line)
            /// returns the first subcommand if passed a nullptr
            App *get_subcommand(const App *subcom) const;

            /// Check to see if a subcommand is part of this command (text version)
            [[nodiscard]] App *get_subcommand(std::string subcom) const;

            /// Get a subcommand by name (noexcept non-const version)
            /// returns null if subcommand doesn't exist
            [[nodiscard]] App *get_subcommand_no_throw(std::string subcom) const noexcept;

            /// Get a pointer to subcommand by index
            [[nodiscard]] App *get_subcommand(int index = 0) const;

            /// Check to see if a subcommand is part of this command and get a shared_ptr to it
            CLI::App_p get_subcommand_ptr(App *subcom) const;

            /// Check to see if a subcommand is part of this command (text version)
            [[nodiscard]] CLI::App_p get_subcommand_ptr(std::string subcom) const;

            /// Get an owning pointer to subcommand by index
            [[nodiscard]] CLI::App_p get_subcommand_ptr(int index = 0) const;

            /// Check to see if an option group is part of this App
            [[nodiscard]] App *get_option_group(std::string group_name) const;

            /// No argument version of count counts the number of times this subcommand was
            /// passed in. The main app will return 1. Unnamed subcommands will also return 1 unless
            /// otherwise modified in a callback
            [[nodiscard]] std::size_t count() const
            {
                return parsed_;
            }

            /// Get a count of all the arguments processed in options and subcommands, this excludes arguments which
            /// were treated as extras.
            [[nodiscard]] std::size_t count_all() const;

            /// Changes the group membership
            App *group(std::string group_name)
            {
                group_ = group_name;
                return this;
            }

            /// The argumentless form of require subcommand requires 1 or more subcommands
            App *require_subcommand()
            {
                require_subcommand_min_ = 1;
                require_subcommand_max_ = 0;
                return this;
            }

            /// Require a subcommand to be given (does not affect help call)
            /// The number required can be given. Negative values indicate maximum
            /// number allowed (0 for any number). Max number inheritable.
            App *require_subcommand(int value)
            {
                if (value < 0)
                {
                    require_subcommand_min_ = 0;
                    require_subcommand_max_ = static_cast<std::size_t>(-value);
                }
                else
                {
                    require_subcommand_min_ = static_cast<std::size_t>(value);
                    require_subcommand_max_ = static_cast<std::size_t>(value);
                }
                return this;
            }

            /// Explicitly control the number of subcommands required. Setting 0
            /// for the max means unlimited number allowed. Max number inheritable.
            App *require_subcommand(std::size_t min, std::size_t max)
            {
                require_subcommand_min_ = min;
                require_subcommand_max_ = max;
                return this;
            }

            /// The argumentless form of require option requires 1 or more options be used
            App *require_option()
            {
                require_option_min_ = 1;
                require_option_max_ = 0;
                return this;
            }

            /// Require an option to be given (does not affect help call)
            /// The number required can be given. Negative values indicate maximum
            /// number allowed (0 for any number).
            App *require_option(int value)
            {
                if (value < 0)
                {
                    require_option_min_ = 0;
                    require_option_max_ = static_cast<std::size_t>(-value);
                }
                else
                {
                    require_option_min_ = static_cast<std::size_t>(value);
                    require_option_max_ = static_cast<std::size_t>(value);
                }
                return this;
            }

            /// Explicitly control the number of options required. Setting 0
            /// for the max means unlimited number allowed. Max number inheritable.
            App *require_option(std::size_t min, std::size_t max)
            {
                require_option_min_ = min;
                require_option_max_ = max;
                return this;
            }

            /// Set fallthrough, set to true so that options will fallthrough to parent if not recognized in a
            /// subcommand Default from parent, usually set on parent.
            App *fallthrough(bool value = true)
            {
                fallthrough_ = value;
                return this;
            }

            /// Set subcommand fallthrough, set to true so that subcommands on parents are recognized
            App *subcommand_fallthrough(bool value = true)
            {
                subcommand_fallthrough_ = value;
                return this;
            }

            /// Check to see if this subcommand was parsed, true only if received on command line.
            /// This allows the subcommand to be directly checked.
            explicit operator bool() const
            {
                return parsed_ > 0;
            }

            ///@}
            /// @name Extras for subclassing
            ///@{

            /// This allows subclasses to inject code before callbacks but after parse.
            ///
            /// This does not run if any errors or help is thrown.
            virtual void pre_callback()
            {
            }

            ///@}
            /// @name Parsing
            ///@{
            //
            /// Reset the parsed data
            void clear();

            /// Parses the command line - throws errors.
            /// This must be called after the options are in but before the rest of the program.
            void parse(int argc, const char *const *argv);
            void parse(int argc, const wchar_t *const *argv);

        private:
            template <class CharT> void parse_char_t(int argc, const CharT *const *argv);

        public:
            /// Parse a single string as if it contained command line arguments.
            /// This function splits the string into arguments then calls parse(std::vector<std::string> &)
            /// the function takes an optional boolean argument specifying if the programName is included in the string
            /// to process
            void parse(std::string commandline, bool program_name_included = false);
            void parse(std::wstring commandline, bool program_name_included = false);

            /// The real work is done here. Expects a reversed vector.
            /// Changes the vector to the remaining options.
            void parse(std::vector<std::string> &args);

            /// The real work is done here. Expects a reversed vector.
            void parse(std::vector<std::string> &&args);

            void parse_from_stream(std::istream &input);

            /// Provide a function to print a help message. The function gets access to the App pointer and error.
            void failure_message(std::function<std::string(const App *, const Error &e)> function)
            {
                failure_message_ = function;
            }

            /// Print a nice error message and return the exit code
            int exit(const Error &e, std::ostream &out = std::cout, std::ostream &err = std::cerr) const;

            ///@}
            /// @name Post parsing
            ///@{

            /// Counts the number of times the given option was passed.
            [[nodiscard]] std::size_t count(std::string option_name) const
            {
                return get_option(option_name)->count();
            }

            /// Get a subcommand pointer list to the currently selected subcommands (after parsing by default, in
            /// command line order; use parsed = false to get the original definition list.)
            [[nodiscard]] std::vector<App *> get_subcommands() const
            {
                return parsed_subcommands_;
            }

            /// Get a filtered subcommand pointer list from the original definition list. An empty function will provide
            /// all subcommands (const)
            std::vector<const App *> get_subcommands(const std::function<bool(const App *)> &filter) const;

            /// Get a filtered subcommand pointer list from the original definition list. An empty function will provide
            /// all subcommands
            std::vector<App *> get_subcommands(const std::function<bool(App *)> &filter);

            /// Check to see if given subcommand was selected
            bool got_subcommand(const App *subcom) const
            {
                // get subcom needed to verify that this was a real subcommand
                return get_subcommand(subcom)->parsed_ > 0;
            }

            /// Check with name instead of pointer to see if subcommand was selected
            [[nodiscard]] bool got_subcommand(std::string subcommand_name) const noexcept
            {
                App *sub = get_subcommand_no_throw(subcommand_name);
                return (sub != nullptr) ? (sub->parsed_ > 0) : false;
            }

            /// Sets excluded options for the subcommand
            App *excludes(Option *opt)
            {
                if (opt == nullptr)
                {
                    throw OptionNotFound("nullptr passed");
                }
                exclude_options_.insert(opt);
                return this;
            }

            /// Sets excluded subcommands for the subcommand
            App *excludes(App *app)
            {
                if (app == nullptr)
                {
                    throw OptionNotFound("nullptr passed");
                }
                if (app == this)
                {
                    throw OptionNotFound("cannot self reference in needs");
                }
                auto res = exclude_subcommands_.insert(app);
                // subcommand exclusion should be symmetric
                if (res.second)
                {
                    app->exclude_subcommands_.insert(this);
                }
                return this;
            }

            App *needs(Option *opt)
            {
                if (opt == nullptr)
                {
                    throw OptionNotFound("nullptr passed");
                }
                need_options_.insert(opt);
                return this;
            }

            App *needs(App *app)
            {
                if (app == nullptr)
                {
                    throw OptionNotFound("nullptr passed");
                }
                if (app == this)
                {
                    throw OptionNotFound("cannot self reference in needs");
                }
                need_subcommands_.insert(app);
                return this;
            }

            /// Removes an option from the excludes list of this subcommand
            bool remove_excludes(Option *opt);

            /// Removes a subcommand from the excludes list of this subcommand
            bool remove_excludes(App *app);

            /// Removes an option from the needs list of this subcommand
            bool remove_needs(Option *opt);

            /// Removes a subcommand from the needs list of this subcommand
            bool remove_needs(App *app);
            ///@}
            /// @name Help
            ///@{

            /// Set usage.
            App *usage(std::string usage_string)
            {
                usage_ = std::move(usage_string);
                return this;
            }
            /// Set usage.
            App *usage(std::function<std::string()> usage_function)
            {
                usage_callback_ = std::move(usage_function);
                return this;
            }
            /// Set footer.
            App *footer(std::string footer_string)
            {
                footer_ = std::move(footer_string);
                return this;
            }
            /// Set footer.
            App *footer(std::function<std::string()> footer_function)
            {
                footer_callback_ = std::move(footer_function);
                return this;
            }
            /// Produce a string that could be read in as a config of the current values of the App. Set default_also to
            /// include default arguments. write_descriptions will print a description for the App and for each option.
            [[nodiscard]] std::string config_to_str() const
            {
                return config_to_str(ConfigOutputMode::Active, false);
            }

            /// Produce a string that could be read in as a config of the current values of the App.
            [[nodiscard]] std::string config_to_str(ConfigOutputMode mode, bool write_description = false) const
            {
                return config_formatter_->to_config(this, mode, write_description, "");
            }

            /// Produce a string that could be read in as a config of the current values of the App. Set default_also to
            /// include default arguments. write_descriptions will print a description for the App and for each option.
            /// This will be deprecated soon, use the version that takes a ConfigOutputMode instead.
            [[nodiscard]] std::string config_to_str(bool default_also, bool write_description = false) const
            {
                return config_to_str(default_also ? ConfigOutputMode::AllDefaults : ConfigOutputMode::Active,
                                     write_description);
            }

            /// Makes a help message, using the currently configured formatter
            /// Will only do one subcommand at a time
            [[nodiscard]] std::string help(std::string prev = "", AppFormatMode mode = AppFormatMode::Normal) const;

            /// Displays a version string
            [[nodiscard]] std::string version() const;
            ///@}
            /// @name Getters
            ///@{

            /// Access the formatter
            [[nodiscard]] std::shared_ptr<FormatterBase> get_formatter() const
            {
                return formatter_;
            }

            /// Access the config formatter
            [[nodiscard]] std::shared_ptr<Config> get_config_formatter() const
            {
                return config_formatter_;
            }

            /// Access the config formatter as a configBase pointer
            [[nodiscard]] std::shared_ptr<ConfigBase> get_config_formatter_base() const
            {
                // This is safer as a dynamic_cast if we have RTTI, as Config -> ConfigBase
                return std::dynamic_pointer_cast<ConfigBase>(config_formatter_);
            }

            /// Get the app or subcommand description
            [[nodiscard]] std::string get_description() const
            {
                return description_;
            }

            /// Set the description of the app
            App *description(std::string app_description)
            {
                description_ = std::move(app_description);
                return this;
            }

            /// Get the list of options (user facing function, so returns raw pointers), has optional filter function
            std::vector<const Option *> get_options(const std::function<bool(const Option *)> filter = {}) const;

            /// Non-const version of the above
            std::vector<Option *> get_options(const std::function<bool(Option *)> filter = {});

            /// Get an option by name (noexcept non-const version)
            [[nodiscard]] Option *get_option_no_throw(std::string option_name) noexcept;

            /// Get an option by name (noexcept const version)
            [[nodiscard]] const Option *get_option_no_throw(std::string option_name) const noexcept;

            /// Get an option by name
            [[nodiscard]] const Option *get_option(std::string option_name) const;

            /// Get an option by name (non-const version)
            [[nodiscard]] Option *get_option(std::string option_name);

            /// Shortcut bracket operator for getting a pointer to an option
            const Option *operator[](const std::string &option_name) const
            {
                return get_option(option_name);
            }

            /// Shortcut bracket operator for getting a pointer to an option
            const Option *operator[](const char *option_name) const
            {
                return get_option(option_name);
            }

            /// Check the status of ignore_case
            [[nodiscard]] bool get_ignore_case() const
            {
                return ignore_case_;
            }

            /// Check the status of ignore_underscore
            [[nodiscard]] bool get_ignore_underscore() const
            {
                return ignore_underscore_;
            }

            /// Check the status of fallthrough
            [[nodiscard]] bool get_fallthrough() const
            {
                return fallthrough_;
            }

            /// Check the status of subcommand fallthrough
            [[nodiscard]] bool get_subcommand_fallthrough() const
            {
                return subcommand_fallthrough_;
            }

            /// Check the status of the allow windows style options
            [[nodiscard]] bool get_allow_windows_style_options() const
            {
                return allow_windows_style_options_;
            }

            /// Check the status of the allow windows style options
            [[nodiscard]] bool get_positionals_at_end() const
            {
                return positionals_at_end_;
            }

            /// Check the status of the allow windows style options
            [[nodiscard]] bool get_configurable() const
            {
                return configurable_;
            }

            /// Get the group of this subcommand
            [[nodiscard]] const std::string &get_group() const
            {
                return group_;
            }

            /// Generate and return the usage.
            [[nodiscard]] std::string get_usage() const
            {
                return (usage_callback_) ? usage_callback_() + '\n' + usage_ : usage_;
            }

            /// Generate and return the footer.
            [[nodiscard]] std::string get_footer() const
            {
                return (footer_callback_) ? footer_callback_() + '\n' + footer_ : footer_;
            }

            /// Get the required min subcommand value
            [[nodiscard]] std::size_t get_require_subcommand_min() const
            {
                return require_subcommand_min_;
            }

            /// Get the required max subcommand value
            [[nodiscard]] std::size_t get_require_subcommand_max() const
            {
                return require_subcommand_max_;
            }

            /// Get the required min option value
            [[nodiscard]] std::size_t get_require_option_min() const
            {
                return require_option_min_;
            }

            /// Get the required max option value
            [[nodiscard]] std::size_t get_require_option_max() const
            {
                return require_option_max_;
            }

            /// Get the prefix command status
            [[nodiscard]] bool get_prefix_command() const
            {
                return static_cast<bool>(prefix_command_);
            }

            /// Get the prefix command status
            [[nodiscard]] PrefixCommandMode get_prefix_command_mode() const
            {
                return prefix_command_;
            }

            /// Get the status of allow extras
            [[nodiscard]] bool get_allow_extras() const
            {
                return allow_extras_ > ExtrasMode::Ignore;
            }

            /// Get the mode of allow_extras
            [[nodiscard]] ExtrasMode get_allow_extras_mode() const
            {
                return allow_extras_;
            }

            /// Get the status of required
            [[nodiscard]] bool get_required() const
            {
                return required_;
            }

            /// Get the status of disabled
            [[nodiscard]] bool get_disabled() const
            {
                return disabled_;
            }

            /// Get the status of silence
            [[nodiscard]] bool get_silent() const
            {
                return silent_;
            }

            /// Get the status of allowing non standard option names
            [[nodiscard]] bool get_allow_non_standard_option_names() const
            {
                return allow_non_standard_options_;
            }

            /// Get the status of allowing prefix matching for subcommands
            [[nodiscard]] bool get_allow_subcommand_prefix_matching() const
            {
                return allow_prefix_matching_;
            }

            /// Get the status of disabled
            [[nodiscard]] bool get_immediate_callback() const
            {
                return immediate_callback_;
            }

            /// Get the status of disabled by default
            [[nodiscard]] bool get_disabled_by_default() const
            {
                return (default_startup == startup_mode::disabled);
            }

            /// Get the status of disabled by default
            [[nodiscard]] bool get_enabled_by_default() const
            {
                return (default_startup == startup_mode::enabled);
            }
            /// Get the status of validating positionals
            [[nodiscard]] bool get_validate_positionals() const
            {
                return validate_positionals_;
            }
            /// Get the status of validating optional vector arguments
            [[nodiscard]] bool get_validate_optional_arguments() const
            {
                return validate_optional_arguments_;
            }

            /// Get the status of allow extras
            [[nodiscard]] config_extras_mode get_allow_config_extras() const
            {
                return static_cast<config_extras_mode>(allow_config_extras_);
            }

            /// Get a pointer to the help flag.
            Option *get_help_ptr()
            {
                return help_ptr_;
            }

            /// Get a pointer to the help flag. (const)
            [[nodiscard]] const Option *get_help_ptr() const
            {
                return help_ptr_;
            }

            /// Get a pointer to the help all flag. (const)
            [[nodiscard]] const Option *get_help_all_ptr() const
            {
                return help_all_ptr_;
            }

            /// Get a pointer to the config option.
            Option *get_config_ptr()
            {
                return config_ptr_;
            }

            /// Get a pointer to the config option. (const)
            [[nodiscard]] const Option *get_config_ptr() const
            {
                return config_ptr_;
            }

            /// Get a pointer to the version option.
            Option *get_version_ptr()
            {
                return version_ptr_;
            }

            /// Get a pointer to the version option. (const)
            [[nodiscard]] const Option *get_version_ptr() const
            {
                return version_ptr_;
            }

            /// Get the parent of this subcommand (or nullptr if main app)
            App *get_parent()
            {
                return parent_;
            }

            /// Get the parent of this subcommand (or nullptr if main app) (const version)
            [[nodiscard]] const App *get_parent() const
            {
                return parent_;
            }

            /// Get the name of the current app
            [[nodiscard]] const std::string &get_name() const
            {
                return name_;
            }

            /// Get the aliases of the current app
            [[nodiscard]] const std::vector<std::string> &get_aliases() const
            {
                return aliases_;
            }

            /// clear all the aliases of the current App
            App *clear_aliases()
            {
                aliases_.clear();
                return this;
            }

            /// Get a display name for an app
            [[nodiscard]] std::string get_display_name(bool with_aliases = false) const;

            /// Check the name, case-insensitive and underscore insensitive, and prefix matching if set
            /// @return true if matched
            [[nodiscard]] bool check_name(std::string name_to_check) const;

            /// @brief  enumeration of matching possibilities
            enum class NameMatch : std::uint8_t
            {
                none = 0,
                exact = 1,
                prefix = 2
            };

            /// Check the name, case-insensitive and underscore insensitive if set
            /// @return NameMatch::none if no match, NameMatch::exact if the match is exact NameMatch::prefix if prefix
            /// is enabled and a prefix matches
            [[nodiscard]] NameMatch check_name_detail(std::string name_to_check) const;

            /// Get the groups available directly from this option (in order)
            [[nodiscard]] std::vector<std::string> get_groups() const;

            /// This gets a vector of pointers with the original parse order
            [[nodiscard]] const std::vector<Option *> &parse_order() const
            {
                return parse_order_;
            }

            /// This returns the missing options from the current subcommand
            [[nodiscard]] std::vector<std::string> remaining(bool recurse = false) const;

            /// This returns the missing options in a form ready for processing by another command line program
            [[nodiscard]] std::vector<std::string> remaining_for_passthrough(bool recurse = false) const;

            /// This returns the number of remaining options, minus the -- separator
            [[nodiscard]] std::size_t remaining_size(bool recurse = false) const;

            ///@}

        protected:
            /// Check the options to make sure there are no conflicts.
            ///
            /// Currently checks to see if multiple positionals exist with unlimited args and checks if the min and max
            /// options are feasible
            void _validate() const;

            /// configure subcommands to enable parsing through the current object
            /// set the correct fallthrough and prefix for nameless subcommands and manage the automatic enable or
            /// disable makes sure parent is set correctly
            void _configure();

            /// Internal function to run (App) callback, bottom up
            void run_callback(bool final_mode = false, bool suppress_final_callback = false);

            /// Check to see if a subcommand is valid. Give up immediately if subcommand max has been reached.
            [[nodiscard]] bool _valid_subcommand(const std::string &current, bool ignore_used = true) const;

            /// Selects a Classifier enum based on the type of the current argument
            [[nodiscard]] detail::Classifier _recognize(const std::string &current,
                                                        bool ignore_used_subcommands = true) const;

            // The parse function is now broken into several parts, and part of process

            /// Read and process a configuration file (main app only)
            void _process_config_file();

            /// Read and process a particular configuration file
            bool _process_config_file(const std::string &config_file, bool throw_error);

            /// Get envname options if not yet passed. Runs on *all* subcommands.
            void _process_env();

            /// Process callbacks. Runs on *all* subcommands.
            void _process_callbacks(CallbackPriority priority);

            /// Run help flag processing if any are found.
            ///
            /// The flags allow recursive calls to remember if there was a help flag on a parent.
            void _process_help_flags(CallbackPriority priority,
                                     bool trigger_help = false,
                                     bool trigger_all_help = false) const;

            /// Verify required options and cross requirements. Subcommands too (only if selected).
            void _process_requirements();

            /// Process callbacks and such.
            void _process();

            /// Throw an error if anything is left over and should not be.
            void _process_extras();

            /// Internal function to recursively increment the parsed counter on the current app as well unnamed
            /// subcommands
            void increment_parsed();

            /// Internal parse function
            void _parse(std::vector<std::string> &args);

            /// Internal parse function
            void _parse(std::vector<std::string> &&args);

            /// Internal function to parse a stream
            void _parse_stream(std::istream &input);

            /// Parse one config param, return false if not found in any subcommand, remove if it is
            ///
            /// If this has more than one dot.separated.name, go into the subcommand matching it
            /// Returns true if it managed to find the option, if false you'll need to remove the arg manually.
            void _parse_config(const std::vector<ConfigItem> &args);

            /// Fill in a single config option
            bool _parse_single_config(const ConfigItem &item, std::size_t level = 0);

            /// @brief store the results for a flag like option
            bool _add_flag_like_result(Option *op, const ConfigItem &item, const std::vector<std::string> &inputs);

            /// Parse "one" argument (some may eat more than one), delegate to parent if fails, add to missing if
            /// missing from main return false if the parse has failed and needs to return to parent
            bool _parse_single(std::vector<std::string> &args, bool &positional_only);

            /// Count the required remaining positional arguments
            [[nodiscard]] std::size_t _count_remaining_positionals(bool required_only = false) const;

            /// Count the required remaining positional arguments
            [[nodiscard]] bool _has_remaining_positionals() const;

            /// Parse a positional, go up the tree to check
            /// @param haltOnSubcommand if set to true the operation will not process subcommands merely return false
            /// Return true if the positional was used false otherwise
            bool _parse_positional(std::vector<std::string> &args, bool haltOnSubcommand);

            /// Locate a subcommand by name with two conditions, should disabled subcommands be ignored, and should used
            /// subcommands be ignored
            [[nodiscard]] App *_find_subcommand(const std::string &subc_name,
                                                bool ignore_disabled,
                                                bool ignore_used) const noexcept;

            /// Parse a subcommand, modify args and continue
            ///
            /// Unlike the others, this one will always allow fallthrough
            /// return true if the subcommand was processed false otherwise
            bool _parse_subcommand(std::vector<std::string> &args);

            /// Parse a short (false) or long (true) argument, must be at the top of the list
            /// if local_processing_only is set to true then fallthrough is disabled will return false if not found
            /// return true if the argument was processed or false if nothing was done
            bool _parse_arg(std::vector<std::string> &args,
                            detail::Classifier current_type,
                            bool local_processing_only);

            /// Trigger the pre_parse callback if needed
            void _trigger_pre_parse(std::size_t remaining_args);

            /// Get the appropriate parent to fallthrough to which is the first one that has a name or the main app
            [[nodiscard]] App *_get_fallthrough_parent() noexcept;

            /// Get the appropriate parent to fallthrough to which is the first one that has a name or the main app
            [[nodiscard]] const App *_get_fallthrough_parent() const noexcept;

            /// Helper function to run through all possible comparisons of subcommand names to check there is no overlap
            [[nodiscard]] const std::string &_compare_subcommand_names(const App &subcom, const App &base) const;

            /// Helper function to place extra values in the most appropriate position
            void _move_to_missing(detail::Classifier val_type, const std::string &val);

        public:
            /// function that could be used by subclasses of App to shift options around into subcommands
            void _move_option(Option *opt, App *app);
    }; // namespace CLI

    /// Extension of App to better manage groups of options
    class Option_group : public App
    {
        public:
            Option_group(std::string group_description, std::string group_name, App *parent)
                : App(std::move(group_description), "", parent)
            {
                group(group_name);
                // option groups should have automatic fallthrough
                if (group_name.empty() || group_name.front() == '+')
                {
                    // help will not be used by default in these contexts
                    set_help_flag("");
                    set_help_all_flag("");
                }
            }
            using App::add_option;
            /// Add an existing option to the Option_group
            Option *add_option(Option *opt)
            {
                if (get_parent() == nullptr)
                {
                    throw OptionNotFound("Unable to locate the specified option");
                }
                get_parent()->_move_option(opt, this);
                return opt;
            }
            /// Add an existing option to the Option_group
            void add_options(Option *opt)
            {
                add_option(opt);
            }
            /// Add a bunch of options to the group
            template <typename... Args> void add_options(Option *opt, Args... args)
            {
                add_option(opt);
                add_options(args...);
            }
            using App::add_subcommand;
            /// Add an existing subcommand to be a member of an option_group
            App *add_subcommand(App *subcom)
            {
                App_p subc = subcom->get_parent()->get_subcommand_ptr(subcom);
                subc->get_parent()->remove_subcommand(subcom);
                add_subcommand(std::move(subc));
                return subcom;
            }
    };

    /// Helper function to enable one option group/subcommand when another is used
    void TriggerOn(App *trigger_app, App *app_to_enable);

    /// Helper function to enable one option group/subcommand when another is used
    void TriggerOn(App *trigger_app, std::vector<App *> apps_to_enable);

    /// Helper function to disable one option group/subcommand when another is used
    void TriggerOff(App *trigger_app, App *app_to_enable);

    /// Helper function to disable one option group/subcommand when another is used
    void TriggerOff(App *trigger_app, std::vector<App *> apps_to_enable);

    /// Helper function to mark an option as deprecated
    void deprecate_option(Option *opt, const std::string &replacement = "");

    /// Helper function to mark an option as deprecated
    void deprecate_option(App *app, const std::string &option_name, const std::string &replacement = "")
    {
        auto *opt = app->get_option(option_name);
        deprecate_option(opt, replacement);
    }

    /// Helper function to mark an option as deprecated
    void deprecate_option(App &app, const std::string &option_name, const std::string &replacement = "")
    {
        auto *opt = app.get_option(option_name);
        deprecate_option(opt, replacement);
    }

    /// Helper function to mark an option as retired
    void retire_option(App *app, Option *opt);

    /// Helper function to mark an option as retired
    void retire_option(App &app, Option *opt);

    /// Helper function to mark an option as retired
    void retire_option(App *app, const std::string &option_name);

    /// Helper function to mark an option as retired
    void retire_option(App &app, const std::string &option_name);

    namespace detail
    {
        /// This class is simply to allow tests access to App's protected functions
        struct AppFriend
        {

                /// Wrap _parse_short, perfectly forward arguments and return
                template <typename... Args> static decltype(auto) parse_arg(App *app, Args &&...args)
                {
                    return app->_parse_arg(std::forward<Args>(args)...);
                }

                /// Wrap _parse_subcommand, perfectly forward arguments and return
                template <typename... Args> static decltype(auto) parse_subcommand(App *app, Args &&...args)
                {
                    return app->_parse_subcommand(std::forward<Args>(args)...);
                }
                /// Wrap the fallthrough parent function to make sure that is working correctly
                static App *get_fallthrough_parent(App *app)
                {
                    return app->_get_fallthrough_parent();
                }

                /// Wrap the const fallthrough parent function to make sure that is working correctly
                static const App *get_fallthrough_parent(const App *app)
                {
                    return app->_get_fallthrough_parent();
                }
        };
    } // namespace detail

    // =============================================
    // Implementation (from App_inl.hpp)
    // =============================================

    App::App(std::string app_description, std::string app_name, App *parent)
        : name_(std::move(app_name)), description_(std::move(app_description)), parent_(parent)
    {
        // Inherit if not from a nullptr
        if (parent_ != nullptr)
        {
            if (parent_->help_ptr_ != nullptr)
                set_help_flag(parent_->help_ptr_->get_name(false, true), parent_->help_ptr_->get_description());
            if (parent_->help_all_ptr_ != nullptr)
                set_help_all_flag(parent_->help_all_ptr_->get_name(false, true),
                                  parent_->help_all_ptr_->get_description());

            /// OptionDefaults
            option_defaults_ = parent_->option_defaults_;

            // INHERITABLE
            failure_message_ = parent_->failure_message_;
            allow_extras_ = parent_->allow_extras_;
            allow_config_extras_ = parent_->allow_config_extras_;
            prefix_command_ = parent_->prefix_command_;
            immediate_callback_ = parent_->immediate_callback_;
            ignore_case_ = parent_->ignore_case_;
            ignore_underscore_ = parent_->ignore_underscore_;
            fallthrough_ = parent_->fallthrough_;
            validate_positionals_ = parent_->validate_positionals_;
            validate_optional_arguments_ = parent_->validate_optional_arguments_;
            configurable_ = parent_->configurable_;
            allow_windows_style_options_ = parent_->allow_windows_style_options_;
            group_ = parent_->group_;
            usage_ = parent_->usage_;
            footer_ = parent_->footer_;
            formatter_ = parent_->formatter_;
            config_formatter_ = parent_->config_formatter_;
            require_subcommand_max_ = parent_->require_subcommand_max_;
            allow_prefix_matching_ = parent_->allow_prefix_matching_;
        }
    }

    [[nodiscard]] char **App::ensure_utf8(char **argv)
    {
#ifdef _WIN32
        (void)argv;

        normalized_argv_ = detail::compute_win32_argv();

        if (!normalized_argv_view_.empty())
        {
            normalized_argv_view_.clear();
        }

        normalized_argv_view_.reserve(normalized_argv_.size());
        for (auto &arg : normalized_argv_)
        {
            // using const_cast is well-defined, string is known to not be const.
            normalized_argv_view_.push_back(const_cast<char *>(arg.data()));
        }

        return normalized_argv_view_.data();
#else
        return argv;
#endif
    }

    App *App::name(std::string app_name)
    {

        if (parent_ != nullptr)
        {
            std::string oname = name_;
            name_ = app_name;
            const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
            if (!res.empty())
            {
                name_ = oname;
                throw(OptionAlreadyAdded(app_name + " conflicts with existing subcommand names"));
            }
        }
        else
        {
            name_ = app_name;
        }
        has_automatic_name_ = false;
        return this;
    }

    App *App::alias(std::string app_name)
    {
        if (app_name.empty() || !detail::valid_alias_name_string(app_name))
        {
            throw IncorrectConstruction("Aliases may not be empty or contain newlines or null characters");
        }
        if (parent_ != nullptr)
        {
            aliases_.push_back(app_name);
            const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
            if (!res.empty())
            {
                aliases_.pop_back();
                throw(OptionAlreadyAdded("alias already matches an existing subcommand: " + app_name));
            }
        }
        else
        {
            aliases_.push_back(app_name);
        }

        return this;
    }

    App *App::immediate_callback(bool immediate)
    {
        immediate_callback_ = immediate;
        if (immediate_callback_)
        {
            if (final_callback_ && !(parse_complete_callback_))
            {
                std::swap(final_callback_, parse_complete_callback_);
            }
        }
        else if (!(final_callback_) && parse_complete_callback_)
        {
            std::swap(final_callback_, parse_complete_callback_);
        }
        return this;
    }

    App *App::ignore_case(bool value)
    {
        if (value && !ignore_case_)
        {
            ignore_case_ = true;
            auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
            const auto &match = _compare_subcommand_names(*this, *p);
            if (!match.empty())
            {
                ignore_case_ = false; // we are throwing so need to be exception invariant
                throw OptionAlreadyAdded("ignore case would cause subcommand name conflicts: " + match);
            }
        }
        ignore_case_ = value;
        return this;
    }

    App *App::ignore_underscore(bool value)
    {
        if (value && !ignore_underscore_)
        {
            ignore_underscore_ = true;
            auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
            const auto &match = _compare_subcommand_names(*this, *p);
            if (!match.empty())
            {
                ignore_underscore_ = false;
                throw OptionAlreadyAdded("ignore underscore would cause subcommand name conflicts: " + match);
            }
        }
        ignore_underscore_ = value;
        return this;
    }

    Option *App::add_option(std::string option_name,
                            callback_t option_callback,
                            std::string option_description,
                            bool defaulted,
                            std::function<std::string()> func)
    {
        Option myopt {option_name, option_description, option_callback, this, allow_non_standard_options_};

        // do a quick search in current subcommand for options
        auto res =
            std::find_if(std::begin(options_), std::end(options_), [&myopt](const Option_p &v) { return *v == myopt; });
        if (res != options_.end())
        {
            const auto &matchname = (*res)->matching_name(myopt);
            throw(OptionAlreadyAdded("added option matched existing option name: " + matchname));
        }
        /** get a top level parent*/
        const App *top_level_parent = this;
        while (top_level_parent->name_.empty() && top_level_parent->parent_ != nullptr)
        {
            top_level_parent = top_level_parent->parent_;
        }

        if (myopt.lnames_.empty() && myopt.snames_.empty())
        {
            // if the option is positional only there is additional potential for ambiguities in config files and needs
            // to be checked
            std::string test_name = "--" + myopt.get_single_name();
            if (test_name.size() == 3)
            {
                test_name.erase(0, 1);
            }
            // if we are in option group
            const auto *op = top_level_parent->get_option_no_throw(test_name);
            if (op != nullptr && op->get_configurable())
            {
                throw(OptionAlreadyAdded("added option positional name matches existing option: " + test_name));
            }
            // need to check if there is another positional with the same name that also doesn't have any long or
            // short names
            op = top_level_parent->get_option_no_throw(myopt.get_single_name());
            if (op != nullptr && op->lnames_.empty() && op->snames_.empty())
            {
                throw(OptionAlreadyAdded("unable to disambiguate with existing option: " + test_name));
            }
        }
        else if (top_level_parent != this)
        {
            for (auto &ln : myopt.lnames_)
            {
                const auto *op = top_level_parent->get_option_no_throw(ln);
                if (op != nullptr && op->get_configurable())
                {
                    throw(OptionAlreadyAdded("added option matches existing positional option: " + ln));
                }
                op = top_level_parent->get_option_no_throw("--" + ln);
                if (op != nullptr && op->get_configurable())
                {
                    throw(OptionAlreadyAdded("added option matches existing option: --" + ln));
                }
                if (ln.size() == 1 || top_level_parent->get_allow_non_standard_option_names())
                {
                    op = top_level_parent->get_option_no_throw("-" + ln);
                    if (op != nullptr && op->get_configurable())
                    {
                        throw(OptionAlreadyAdded("added option matches existing option: -" + ln));
                    }
                }
            }
            for (auto &sn : myopt.snames_)
            {
                const auto *op = top_level_parent->get_option_no_throw(sn);
                if (op != nullptr && op->get_configurable())
                {
                    throw(OptionAlreadyAdded("added option matches existing positional option: " + sn));
                }
                op = top_level_parent->get_option_no_throw("-" + sn);
                if (op != nullptr && op->get_configurable())
                {
                    throw(OptionAlreadyAdded("added option matches existing option: -" + sn));
                }
                op = top_level_parent->get_option_no_throw("--" + sn);
                if (op != nullptr && op->get_configurable())
                {
                    throw(OptionAlreadyAdded("added option matches existing option: --" + sn));
                }
            }
        }
        if (allow_non_standard_options_ && !myopt.snames_.empty())
        {

            for (auto &sname : myopt.snames_)
            {
                if (sname.length() > 1)
                {
                    std::string test_name;
                    test_name.push_back('-');
                    test_name.push_back(sname.front());
                    const auto *op = top_level_parent->get_option_no_throw(test_name);
                    if (op != nullptr)
                    {
                        throw(OptionAlreadyAdded("added option interferes with existing short option: " + sname));
                    }
                }
            }
            for (auto &opt : top_level_parent->get_options())
            {
                for (const auto &osn : opt->snames_)
                {
                    if (osn.size() > 1)
                    {
                        std::string test_name;
                        test_name.push_back(osn.front());
                        if (myopt.check_sname(test_name))
                        {
                            throw(OptionAlreadyAdded("added option interferes with existing non standard option: " +
                                                     osn));
                        }
                    }
                }
            }
        }
        options_.emplace_back();
        Option_p &option = options_.back();
        option.reset(new Option(option_name, option_description, option_callback, this, allow_non_standard_options_));

        // Set the default string capture function
        option->default_function(func);

        // For compatibility with CLI11 1.7 and before, capture the default string here
        if (defaulted)
            option->capture_default_str();

        // Transfer defaults to the new option
        option_defaults_.copy_to(option.get());

        // Don't bother to capture if we already did
        if (!defaulted && option->get_always_capture_default())
            option->capture_default_str();

        return option.get();
    }

    Option *App::set_help_flag(std::string flag_name, const std::string &help_description)
    {
        // take flag_description by const reference otherwise add_flag tries to assign to help_description
        if (help_ptr_ != nullptr)
        {
            remove_option(help_ptr_);
            help_ptr_ = nullptr;
        }

        // Empty name will simply remove the help flag
        if (!flag_name.empty())
        {
            help_ptr_ = add_flag(flag_name, help_description);
            help_ptr_->configurable(false)->callback_priority(CallbackPriority::First);
        }

        return help_ptr_;
    }

    Option *App::set_help_all_flag(std::string help_name, const std::string &help_description)
    {
        // take flag_description by const reference otherwise add_flag tries to assign to flag_description
        if (help_all_ptr_ != nullptr)
        {
            remove_option(help_all_ptr_);
            help_all_ptr_ = nullptr;
        }

        // Empty name will simply remove the help all flag
        if (!help_name.empty())
        {
            help_all_ptr_ = add_flag(help_name, help_description);
            help_all_ptr_->configurable(false)->callback_priority(CallbackPriority::First);
        }

        return help_all_ptr_;
    }

    Option *App::set_version_flag(std::string flag_name,
                                  const std::string &versionString,
                                  const std::string &version_help)
    {
        // take flag_description by const reference otherwise add_flag tries to assign to version_description
        if (version_ptr_ != nullptr)
        {
            remove_option(version_ptr_);
            version_ptr_ = nullptr;
        }

        // Empty name will simply remove the version flag
        if (!flag_name.empty())
        {
            version_ptr_ = add_flag_callback(
                flag_name, [versionString]() { throw(CLI::CallForVersion(versionString, 0)); }, version_help);
            version_ptr_->configurable(false)->callback_priority(CallbackPriority::First);
        }

        return version_ptr_;
    }

    Option *App::set_version_flag(std::string flag_name,
                                  std::function<std::string()> vfunc,
                                  const std::string &version_help)
    {
        if (version_ptr_ != nullptr)
        {
            remove_option(version_ptr_);
            version_ptr_ = nullptr;
        }

        // Empty name will simply remove the version flag
        if (!flag_name.empty())
        {
            version_ptr_ =
                add_flag_callback(flag_name, [vfunc]() { throw(CLI::CallForVersion(vfunc(), 0)); }, version_help);
            version_ptr_->configurable(false)->callback_priority(CallbackPriority::First);
        }

        return version_ptr_;
    }

    Option *App::_add_flag_internal(std::string flag_name, CLI::callback_t fun, std::string flag_description)
    {
        Option *opt = nullptr;
        if (detail::has_default_flag_values(flag_name))
        {
            // check for default values and if it has them
            auto flag_defaults = detail::get_default_flag_values(flag_name);
            detail::remove_default_flag_values(flag_name);
            opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
            for (const auto &fname : flag_defaults)
                opt->fnames_.push_back(fname.first);
            opt->default_flag_values_ = std::move(flag_defaults);
        }
        else
        {
            opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
        }
        // flags cannot have positional values
        if (opt->get_positional())
        {
            auto pos_name = opt->get_name(true);
            remove_option(opt);
            throw IncorrectConstruction::PositionalFlag(pos_name);
        }
        opt->multi_option_policy(MultiOptionPolicy::TakeLast);
        opt->expected(0);
        opt->required(false);
        return opt;
    }

    Option *App::add_flag_callback(std::string flag_name,
                                   std::function<void(void)> function, ///< A function to call, void(void)
                                   std::string flag_description)
    {

        CLI::callback_t fun = [function](const CLI::results_t &res) {
            using CLI::detail::lexical_cast;
            bool trigger {false};
            auto result = lexical_cast(res[0], trigger);
            if (result && trigger)
            {
                function();
            }
            return result;
        };
        return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description));
    }

    Option *App::add_flag_function(std::string flag_name,
                                   std::function<void(std::int64_t)> function, ///< A function to call, void(int)
                                   std::string flag_description)
    {

        CLI::callback_t fun = [function](const CLI::results_t &res) {
            using CLI::detail::lexical_cast;
            std::int64_t flag_count {0};
            lexical_cast(res[0], flag_count);
            function(flag_count);
            return true;
        };
        return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description))
            ->multi_option_policy(MultiOptionPolicy::Sum);
    }

    Option *App::set_config(std::string option_name,
                            std::string default_filename,
                            const std::string &help_message,
                            bool config_required)
    {

        // Remove existing config if present
        if (config_ptr_ != nullptr)
        {
            remove_option(config_ptr_);
            config_ptr_ = nullptr; // need to remove the config_ptr completely
        }

        // Only add config if option passed
        if (!option_name.empty())
        {
            config_ptr_ = add_option(option_name, help_message);
            if (config_required)
            {
                config_ptr_->required();
            }
            if (!default_filename.empty())
            {
                config_ptr_->default_str(std::move(default_filename));
                config_ptr_->force_callback_ = true;
            }
            config_ptr_->configurable(false);
            // set the option to take the last value and reverse given by default
            config_ptr_->multi_option_policy(MultiOptionPolicy::Reverse);
        }

        return config_ptr_;
    }

    bool App::remove_option(Option *opt)
    {
        // Make sure no links exist
        for (Option_p &op : options_)
        {
            op->remove_needs(opt);
            op->remove_excludes(opt);
        }

        if (help_ptr_ == opt)
            help_ptr_ = nullptr;
        if (help_all_ptr_ == opt)
            help_all_ptr_ = nullptr;
        if (config_ptr_ == opt)
            config_ptr_ = nullptr;

        auto iterator =
            std::find_if(std::begin(options_), std::end(options_), [opt](const Option_p &v) { return v.get() == opt; });
        if (iterator != std::end(options_))
        {
            options_.erase(iterator);
            return true;
        }
        return false;
    }

    App *App::add_subcommand(std::string subcommand_name, std::string subcommand_description)
    {
        if (!subcommand_name.empty() && !detail::valid_name_string(subcommand_name))
        {
            if (!detail::valid_first_char(subcommand_name[0]))
            {
                throw IncorrectConstruction(
                    "Subcommand name starts with invalid character, '!' and '-' and control characters");
            }
            for (auto c : subcommand_name)
            {
                if (!detail::valid_later_char(c))
                {
                    throw IncorrectConstruction(std::string("Subcommand name contains invalid character ('") + c +
                                                "'), all characters are allowed except"
                                                "'=',':','{','}', ' ', and control characters");
                }
            }
        }
        CLI::App_p subcom = std::shared_ptr<App>(new App(std::move(subcommand_description), subcommand_name, this));
        return add_subcommand(std::move(subcom));
    }

    App *App::add_subcommand(CLI::App_p subcom)
    {
        if (!subcom)
            throw IncorrectConstruction("passed App is not valid");
        auto *ckapp = (name_.empty() && parent_ != nullptr) ? _get_fallthrough_parent() : this;
        const auto &mstrg = _compare_subcommand_names(*subcom, *ckapp);
        if (!mstrg.empty())
        {
            throw(OptionAlreadyAdded("subcommand name or alias matches existing subcommand: " + mstrg));
        }
        subcom->parent_ = this;
        subcommands_.push_back(std::move(subcom));
        return subcommands_.back().get();
    }

    bool App::remove_subcommand(App *subcom)
    {
        // Make sure no links exist
        for (App_p &sub : subcommands_)
        {
            sub->remove_excludes(subcom);
            sub->remove_needs(subcom);
        }

        auto iterator = std::find_if(
            std::begin(subcommands_), std::end(subcommands_), [subcom](const App_p &v) { return v.get() == subcom; });
        if (iterator != std::end(subcommands_))
        {
            subcommands_.erase(iterator);
            return true;
        }
        return false;
    }

    App *App::get_subcommand(const App *subcom) const
    {
        if (subcom == nullptr)
            throw OptionNotFound("nullptr passed");
        for (const App_p &subcomptr : subcommands_)
            if (subcomptr.get() == subcom)
                return subcomptr.get();
        throw OptionNotFound(subcom->get_name());
    }

    [[nodiscard]] App *App::get_subcommand(std::string subcom) const
    {
        auto *subc = _find_subcommand(subcom, false, false);
        if (subc == nullptr)
            throw OptionNotFound(subcom);
        return subc;
    }

    [[nodiscard]] App *App::get_subcommand_no_throw(std::string subcom) const noexcept
    {
        return _find_subcommand(subcom, false, false);
    }

    [[nodiscard]] App *App::get_subcommand(int index) const
    {
        if (index >= 0)
        {
            auto uindex = static_cast<unsigned>(index);
            if (uindex < subcommands_.size())
                return subcommands_[uindex].get();
        }
        throw OptionNotFound(std::to_string(index));
    }

    CLI::App_p App::get_subcommand_ptr(App *subcom) const
    {
        if (subcom == nullptr)
            throw OptionNotFound("nullptr passed");
        for (const App_p &subcomptr : subcommands_)
            if (subcomptr.get() == subcom)
                return subcomptr;
        throw OptionNotFound(subcom->get_name());
    }

    [[nodiscard]] CLI::App_p App::get_subcommand_ptr(std::string subcom) const
    {
        for (const App_p &subcomptr : subcommands_)
            if (subcomptr->check_name(subcom))
                return subcomptr;
        throw OptionNotFound(subcom);
    }

    [[nodiscard]] CLI::App_p App::get_subcommand_ptr(int index) const
    {
        if (index >= 0)
        {
            auto uindex = static_cast<unsigned>(index);
            if (uindex < subcommands_.size())
                return subcommands_[uindex];
        }
        throw OptionNotFound(std::to_string(index));
    }

    [[nodiscard]] CLI::App *App::get_option_group(std::string group_name) const
    {
        for (const App_p &app : subcommands_)
        {
            if (app->name_.empty() && app->group_ == group_name)
            {
                return app.get();
            }
        }
        throw OptionNotFound(group_name);
    }

    [[nodiscard]] std::size_t App::count_all() const
    {
        std::size_t cnt {0};
        for (const auto &opt : options_)
        {
            cnt += opt->count();
        }
        for (const auto &sub : subcommands_)
        {
            cnt += sub->count_all();
        }
        if (!get_name().empty())
        { // for named subcommands add the number of times the subcommand was called
            cnt += parsed_;
        }
        return cnt;
    }

    void App::clear()
    {

        parsed_ = 0;
        pre_parse_called_ = false;

        missing_.clear();
        parsed_subcommands_.clear();
        parse_order_.clear();
        for (const Option_p &opt : options_)
        {
            opt->clear();
        }
        for (const App_p &subc : subcommands_)
        {
            subc->clear();
        }
    }

    void App::parse(int argc, const char *const *argv)
    {
        parse_char_t(argc, argv);
    }
    void App::parse(int argc, const wchar_t *const *argv)
    {
        parse_char_t(argc, argv);
    }

    namespace detail
    {

        // Do nothing or perform narrowing
        const char *maybe_narrow(const char *str)
        {
            return str;
        }
        std::string maybe_narrow(const wchar_t *str)
        {
            return narrow(str);
        }

    } // namespace detail

    template <class CharT> void App::parse_char_t(int argc, const CharT *const *argv)
    {
        // If the name is not set, read from command line
        if (name_.empty() || has_automatic_name_)
        {
            has_automatic_name_ = true;
            name_ = detail::maybe_narrow(argv[0]);
        }

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc) - 1U);
        for (auto i = static_cast<std::size_t>(argc) - 1U; i > 0U; --i)
            args.emplace_back(detail::maybe_narrow(argv[i]));

        parse(std::move(args));
    }

    void App::parse(std::string commandline, bool program_name_included)
    {

        if (program_name_included)
        {
            auto nstr = detail::split_program_name(commandline);
            if ((name_.empty()) || (has_automatic_name_))
            {
                has_automatic_name_ = true;
                name_ = nstr.first;
            }
            commandline = std::move(nstr.second);
        }
        else
        {
            detail::trim(commandline);
        }
        // the next section of code is to deal with quoted arguments after an '=' or ':' for windows like operations
        if (!commandline.empty())
        {
            commandline = detail::find_and_modify(commandline, "=", detail::escape_detect);
            if (allow_windows_style_options_)
                commandline = detail::find_and_modify(commandline, ":", detail::escape_detect);
        }

        auto args = detail::split_up(std::move(commandline));
        // remove all empty strings
        args.erase(std::remove(args.begin(), args.end(), std::string {}), args.end());
        try
        {
            detail::remove_quotes(args);
        }
        catch (const std::invalid_argument &arg)
        {
            throw CLI::ParseError(arg.what(), CLI::ExitCodes::InvalidError);
        }
        std::reverse(args.begin(), args.end());
        parse(std::move(args));
    }

    void App::parse(std::wstring commandline, bool program_name_included)
    {
        parse(narrow(commandline), program_name_included);
    }

    void App::parse(std::vector<std::string> &args)
    {
        // Clear if parsed
        if (parsed_ > 0)
            clear();

        // parsed_ is incremented in commands/subcommands,
        // but placed here to make sure this is cleared when
        // running parse after an error is thrown, even by _validate or _configure.
        parsed_ = 1;
        _validate();
        _configure();
        // set the parent as nullptr as this object should be the top now
        parent_ = nullptr;
        parsed_ = 0;

        _parse(args);
        run_callback();
    }

    void App::parse(std::vector<std::string> &&args)
    {
        // Clear if parsed
        if (parsed_ > 0)
            clear();

        // parsed_ is incremented in commands/subcommands,
        // but placed here to make sure this is cleared when
        // running parse after an error is thrown, even by _validate or _configure.
        parsed_ = 1;
        _validate();
        _configure();
        // set the parent as nullptr as this object should be the top now
        parent_ = nullptr;
        parsed_ = 0;

        _parse(std::move(args));
        run_callback();
    }

    void App::parse_from_stream(std::istream &input)
    {
        if (parsed_ == 0)
        {
            _validate();
            _configure();
            // set the parent as nullptr as this object should be the top now
        }

        _parse_stream(input);
        run_callback();
    }

    int App::exit(const Error &e, std::ostream &out, std::ostream &err) const
    {

        /// Avoid printing anything if this is a CLI::RuntimeError
        if (e.get_name() == "RuntimeError")
            return e.get_exit_code();

        if (e.get_name() == "CallForHelp")
        {
            out << help();
            return e.get_exit_code();
        }

        if (e.get_name() == "CallForAllHelp")
        {
            out << help("", AppFormatMode::All);
            return e.get_exit_code();
        }

        if (e.get_name() == "CallForVersion")
        {
            out << e.what() << '\n';
            return e.get_exit_code();
        }

        if (e.get_exit_code() != static_cast<int>(ExitCodes::Success))
        {
            if (failure_message_)
                err << failure_message_(this, e) << std::flush;
        }

        return e.get_exit_code();
    }

    std::vector<const App *> App::get_subcommands(const std::function<bool(const App *)> &filter) const
    {
        std::vector<const App *> subcomms(subcommands_.size());
        std::transform(std::begin(subcommands_), std::end(subcommands_), std::begin(subcomms), [](const App_p &v) {
            return v.get();
        });

        if (filter)
        {
            subcomms.erase(std::remove_if(std::begin(subcomms),
                                          std::end(subcomms),
                                          [&filter](const App *app) { return !filter(app); }),
                           std::end(subcomms));
        }

        return subcomms;
    }

    std::vector<App *> App::get_subcommands(const std::function<bool(App *)> &filter)
    {
        std::vector<App *> subcomms(subcommands_.size());
        std::transform(std::begin(subcommands_), std::end(subcommands_), std::begin(subcomms), [](const App_p &v) {
            return v.get();
        });

        if (filter)
        {
            subcomms.erase(
                std::remove_if(std::begin(subcomms), std::end(subcomms), [&filter](App *app) { return !filter(app); }),
                std::end(subcomms));
        }

        return subcomms;
    }

    bool App::remove_excludes(Option *opt)
    {
        auto iterator = std::find(std::begin(exclude_options_), std::end(exclude_options_), opt);
        if (iterator == std::end(exclude_options_))
        {
            return false;
        }
        exclude_options_.erase(iterator);
        return true;
    }

    bool App::remove_excludes(App *app)
    {
        auto iterator = std::find(std::begin(exclude_subcommands_), std::end(exclude_subcommands_), app);
        if (iterator == std::end(exclude_subcommands_))
        {
            return false;
        }
        auto *other_app = *iterator;
        exclude_subcommands_.erase(iterator);
        other_app->remove_excludes(this);
        return true;
    }

    bool App::remove_needs(Option *opt)
    {
        auto iterator = std::find(std::begin(need_options_), std::end(need_options_), opt);
        if (iterator == std::end(need_options_))
        {
            return false;
        }
        need_options_.erase(iterator);
        return true;
    }

    bool App::remove_needs(App *app)
    {
        auto iterator = std::find(std::begin(need_subcommands_), std::end(need_subcommands_), app);
        if (iterator == std::end(need_subcommands_))
        {
            return false;
        }
        need_subcommands_.erase(iterator);
        return true;
    }

    [[nodiscard]] std::string App::help(std::string prev, AppFormatMode mode) const
    {
        if (prev.empty())
            prev = get_name();
        else
            prev += " " + get_name();

        // Delegate to subcommand if needed
        auto selected_subcommands = get_subcommands();
        if (!selected_subcommands.empty())
        {
            return selected_subcommands.back()->help(prev, mode);
        }
        return formatter_->make_help(this, prev, mode);
    }

    [[nodiscard]] std::string App::version() const
    {
        std::string val;
        if (version_ptr_ != nullptr)
        {
            // copy the results for reuse later
            results_t rv = version_ptr_->results();
            version_ptr_->clear();
            version_ptr_->add_result("true");
            try
            {
                version_ptr_->run_callback();
            }
            catch (const CLI::CallForVersion &cfv)
            {
                val = cfv.what();
            }
            version_ptr_->clear();
            version_ptr_->add_result(rv);
        }
        return val;
    }

    std::vector<const Option *> App::get_options(const std::function<bool(const Option *)> filter) const
    {
        std::vector<const Option *> options(options_.size());
        std::transform(std::begin(options_), std::end(options_), std::begin(options), [](const Option_p &val) {
            return val.get();
        });

        if (filter)
        {
            options.erase(std::remove_if(std::begin(options),
                                         std::end(options),
                                         [&filter](const Option *opt) { return !filter(opt); }),
                          std::end(options));
        }
        for (const auto &subcp : subcommands_)
        {
            // also check down into nameless subcommands
            const App *subc = subcp.get();
            if (subc->get_name().empty() && !subc->get_group().empty() && subc->get_group().front() == '+')
            {
                std::vector<const Option *> subcopts = subc->get_options(filter);
                options.insert(options.end(), subcopts.begin(), subcopts.end());
            }
        }
        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            const auto *fallthrough_parent = _get_fallthrough_parent();
            std::vector<const Option *> subcopts = fallthrough_parent->get_options(filter);
            for (const auto *opt : subcopts)
            {
                if (std::find_if(options.begin(), options.end(), [opt](const Option *opt2) {
                        return opt->check_name(opt2->get_name());
                    }) == options.end())
                {
                    options.push_back(opt);
                }
            }
        }
        return options;
    }

    std::vector<Option *> App::get_options(const std::function<bool(Option *)> filter)
    {
        std::vector<Option *> options(options_.size());
        std::transform(std::begin(options_), std::end(options_), std::begin(options), [](const Option_p &val) {
            return val.get();
        });

        if (filter)
        {
            options.erase(
                std::remove_if(std::begin(options), std::end(options), [&filter](Option *opt) { return !filter(opt); }),
                std::end(options));
        }
        for (auto &subc : subcommands_)
        {
            // also check down into nameless subcommands and specific groups
            if (subc->get_name().empty() || (!subc->get_group().empty() && subc->get_group().front() == '+'))
            {
                auto subcopts = subc->get_options(filter);
                options.insert(options.end(), subcopts.begin(), subcopts.end());
            }
        }
        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            auto *fallthrough_parent = _get_fallthrough_parent();
            std::vector<Option *> subcopts = fallthrough_parent->get_options(filter);
            for (auto *opt : subcopts)
            {
                if (std::find_if(options.begin(), options.end(), [opt](Option *opt2) {
                        return opt->check_name(opt2->get_name());
                    }) == options.end())
                {
                    options.push_back(opt);
                }
            }
        }
        return options;
    }

    /// Get an option by name
    [[nodiscard]] const Option *App::get_option(std::string option_name) const
    {
        const auto *opt = get_option_no_throw(option_name);
        if (opt == nullptr)
        {
            if (fallthrough_ && parent_ != nullptr && name_.empty())
            {
                // as a special case option groups with fallthrough enabled can also check the parent for options if the
                // option is not found in the group this will not recurse as the internal call is to the no_throw
                // version which will not check the parent again for option groups even with fallthrough enabled
                return _get_fallthrough_parent()->get_option(option_name);
            }
            throw OptionNotFound(option_name);
        }
        return opt;
    }

    /// Get an option by name (non-const version)
    [[nodiscard]] Option *App::get_option(std::string option_name)
    {
        auto *opt = get_option_no_throw(option_name);
        if (opt == nullptr)
        {
            if (fallthrough_ && parent_ != nullptr && name_.empty())
            {
                // as a special case option groups with fallthrough enabled can also check the parent for options if the
                // option is not found in the group this will not recurse as the internal call is to the no_throw
                // version which will not check the parent again for option groups even with fallthrough enabled
                return _get_fallthrough_parent()->get_option(option_name);
            }
            throw OptionNotFound(option_name);
        }
        return opt;
    }

    [[nodiscard]] Option *App::get_option_no_throw(std::string option_name) noexcept
    {
        for (Option_p &opt : options_)
        {
            if (opt->check_name(option_name))
            {
                return opt.get();
            }
        }
        for (auto &subc : subcommands_)
        {
            // also check down into nameless subcommands
            if (subc->get_name().empty())
            {
                auto *opt = subc->get_option_no_throw(option_name);
                if (opt != nullptr)
                {
                    return opt;
                }
            }
        }
        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            // if there is fallthrough and a parent and this is not an option_group then also check the parent for the
            // option
            return _get_fallthrough_parent()->get_option_no_throw(option_name);
        }
        return nullptr;
    }

    [[nodiscard]] const Option *App::get_option_no_throw(std::string option_name) const noexcept
    {
        for (const Option_p &opt : options_)
        {
            if (opt->check_name(option_name))
            {
                return opt.get();
            }
        }
        for (const auto &subc : subcommands_)
        {
            // also check down into nameless subcommands
            if (subc->get_name().empty())
            {
                auto *opt = subc->get_option_no_throw(option_name);
                if (opt != nullptr)
                {
                    return opt;
                }
            }
        }
        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            return _get_fallthrough_parent()->get_option_no_throw(option_name);
        }
        return nullptr;
    }

    [[nodiscard]] std::string App::get_display_name(bool with_aliases) const
    {
        if (name_.empty())
        {
            return std::string("[Option Group: ") + get_group() + "]";
        }
        if (aliases_.empty() || !with_aliases)
        {
            return name_;
        }
        std::string dispname = name_;
        for (const auto &lalias : aliases_)
        {
            dispname.push_back(',');
            dispname.push_back(' ');
            dispname.append(lalias);
        }
        return dispname;
    }

    [[nodiscard]] bool App::check_name(std::string name_to_check) const
    {
        auto result = check_name_detail(std::move(name_to_check));
        return (result != NameMatch::none);
    }

    [[nodiscard]] App::NameMatch App::check_name_detail(std::string name_to_check) const
    {
        std::string local_name = name_;
        if (ignore_underscore_)
        {
            local_name = detail::remove_underscore(name_);
            name_to_check = detail::remove_underscore(name_to_check);
        }
        if (ignore_case_)
        {
            local_name = detail::to_lower(name_);
            name_to_check = detail::to_lower(name_to_check);
        }

        if (local_name == name_to_check)
        {
            return App::NameMatch::exact;
        }
        if (allow_prefix_matching_ && name_to_check.size() < local_name.size())
        {
            if (local_name.compare(0, name_to_check.size(), name_to_check) == 0)
            {
                return App::NameMatch::prefix;
            }
        }
        for (std::string les : aliases_)
        { // NOLINT(performance-for-range-copy)
            if (ignore_underscore_)
            {
                les = detail::remove_underscore(les);
            }
            if (ignore_case_)
            {
                les = detail::to_lower(les);
            }
            if (les == name_to_check)
            {
                return App::NameMatch::exact;
            }
            if (allow_prefix_matching_ && name_to_check.size() < les.size())
            {
                if (les.compare(0, name_to_check.size(), name_to_check) == 0)
                {
                    return App::NameMatch::prefix;
                }
            }
        }
        return App::NameMatch::none;
    }

    [[nodiscard]] std::vector<std::string> App::get_groups() const
    {
        std::vector<std::string> groups;

        for (const Option_p &opt : options_)
        {
            // Add group if it is not already in there
            if (std::find(groups.begin(), groups.end(), opt->get_group()) == groups.end())
            {
                groups.push_back(opt->get_group());
            }
        }

        return groups;
    }

    [[nodiscard]] std::vector<std::string> App::remaining(bool recurse) const
    {
        std::vector<std::string> miss_list;
        for (const std::pair<detail::Classifier, std::string> &miss : missing_)
        {
            miss_list.push_back(std::get<1>(miss));
        }
        // Get from a subcommand that may allow extras
        if (recurse)
        {
            if (allow_extras_ == ExtrasMode::Error || allow_extras_ == ExtrasMode::Ignore)
            {
                for (const auto &sub : subcommands_)
                {
                    if (sub->name_.empty() && !sub->missing_.empty())
                    {
                        for (const std::pair<detail::Classifier, std::string> &miss : sub->missing_)
                        {
                            miss_list.push_back(std::get<1>(miss));
                        }
                    }
                }
            }
            // Recurse into subcommands

            for (const App *sub : parsed_subcommands_)
            {
                std::vector<std::string> output = sub->remaining(recurse);
                std::copy(std::begin(output), std::end(output), std::back_inserter(miss_list));
            }
        }
        return miss_list;
    }

    [[nodiscard]] std::vector<std::string> App::remaining_for_passthrough(bool recurse) const
    {
        std::vector<std::string> miss_list = remaining(recurse);
        std::reverse(std::begin(miss_list), std::end(miss_list));
        return miss_list;
    }

    [[nodiscard]] std::size_t App::remaining_size(bool recurse) const
    {
        auto remaining_options = static_cast<std::size_t>(std::count_if(
            std::begin(missing_), std::end(missing_), [](const std::pair<detail::Classifier, std::string> &val) {
                return val.first != detail::Classifier::POSITIONAL_MARK;
            }));

        if (recurse)
        {
            for (const App_p &sub : subcommands_)
            {
                remaining_options += sub->remaining_size(recurse);
            }
        }
        return remaining_options;
    }

    void App::_validate() const
    {
        // count the number of positional only args
        auto pcount = std::count_if(std::begin(options_), std::end(options_), [](const Option_p &opt) {
            return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional();
        });
        if (pcount > 1)
        {
            auto pcount_req = std::count_if(std::begin(options_), std::end(options_), [](const Option_p &opt) {
                return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional() &&
                       opt->get_required();
            });
            if (pcount - pcount_req > 1)
            {
                throw InvalidError(name_);
            }
        }

        std::size_t nameless_subs {0};
        for (const App_p &app : subcommands_)
        {
            app->_validate();
            if (app->get_name().empty())
                ++nameless_subs;
        }

        if (require_option_min_ > 0)
        {
            if (require_option_max_ > 0)
            {
                if (require_option_max_ < require_option_min_)
                {
                    throw(InvalidError("Required min options greater than required max options",
                                       ExitCodes::InvalidError));
                }
            }
            if (require_option_min_ > (options_.size() + nameless_subs))
            {
                throw(InvalidError("Required min options greater than number of available options",
                                   ExitCodes::InvalidError));
            }
        }
    }

    void App::_configure()
    {
        if (default_startup == startup_mode::enabled)
        {
            disabled_ = false;
        }
        else if (default_startup == startup_mode::disabled)
        {
            disabled_ = true;
        }
        for (const App_p &app : subcommands_)
        {
            if (app->has_automatic_name_)
            {
                app->name_.clear();
            }
            if (app->name_.empty())
            {
                app->fallthrough_ = false; // make sure fallthrough_ is false to prevent infinite loop
                app->prefix_command_ = PrefixCommandMode::Off;
            }
            // make sure the parent is set to be this object in preparation for parse
            app->parent_ = this;
            app->_configure();
        }
    }

    void App::run_callback(bool final_mode, bool suppress_final_callback)
    {
        pre_callback();
        // in the main app if immediate_callback_ is set it runs the main callback before the used subcommands
        if (!final_mode && parse_complete_callback_)
        {
            parse_complete_callback_();
        }
        // run the callbacks for the received subcommands
        for (App *subc : get_subcommands())
        {
            if (subc->parent_ == this)
            {
                subc->run_callback(true, suppress_final_callback);
            }
        }
        // now run callbacks for option_groups
        for (auto &subc : subcommands_)
        {
            if (subc->name_.empty() && subc->count_all() > 0)
            {
                subc->run_callback(true, suppress_final_callback);
            }
        }

        // finally run the main callback
        if (final_callback_ && (parsed_ > 0) && (!suppress_final_callback))
        {
            if (!name_.empty() || count_all() > 0 || parent_ == nullptr)
            {
                final_callback_();
            }
        }
    }

    [[nodiscard]] bool App::_valid_subcommand(const std::string &current, bool ignore_used) const
    {
        // Don't match if max has been reached - but still check parents
        if (require_subcommand_max_ != 0 && parsed_subcommands_.size() >= require_subcommand_max_ &&
            subcommand_fallthrough_)
        {
            return parent_ != nullptr && parent_->_valid_subcommand(current, ignore_used);
        }
        auto *com = _find_subcommand(current, true, ignore_used);
        if (com != nullptr)
        {
            return true;
        }
        // Check parent if exists, else return false
        if (subcommand_fallthrough_)
        {
            return parent_ != nullptr && parent_->_valid_subcommand(current, ignore_used);
        }
        return false;
    }

    [[nodiscard]] detail::Classifier App::_recognize(const std::string &current, bool ignore_used_subcommands) const
    {
        std::string dummy1, dummy2;

        if (current == "--")
            return detail::Classifier::POSITIONAL_MARK;
        if (_valid_subcommand(current, ignore_used_subcommands))
            return detail::Classifier::SUBCOMMAND;
        if (detail::split_long(current, dummy1, dummy2))
            return detail::Classifier::LONG;
        if (detail::split_short(current, dummy1, dummy2))
        {
            if ((dummy1[0] >= '0' && dummy1[0] <= '9') ||
                (dummy1[0] == '.' && !dummy2.empty() && (dummy2[0] >= '0' && dummy2[0] <= '9')))
            {
                // it looks like a number but check if it could be an option
                if (get_option_no_throw(std::string {'-', dummy1[0]}) == nullptr)
                {
                    return detail::Classifier::NONE;
                }
            }
            return detail::Classifier::SHORT;
        }
        if ((allow_windows_style_options_) && (detail::split_windows_style(current, dummy1, dummy2)))
            return detail::Classifier::WINDOWS_STYLE;
        if ((current == "++") && !name_.empty() && parent_ != nullptr)
            return detail::Classifier::SUBCOMMAND_TERMINATOR;
        auto dotloc = current.find_first_of('.');
        if (dotloc != std::string::npos)
        {
            auto *cm = _find_subcommand(current.substr(0, dotloc), true, ignore_used_subcommands);
            if (cm != nullptr)
            {
                auto res = cm->_recognize(current.substr(dotloc + 1), ignore_used_subcommands);
                if (res == detail::Classifier::SUBCOMMAND)
                {
                    return res;
                }
            }
        }
        return detail::Classifier::NONE;
    }

    bool App::_process_config_file(const std::string &config_file, bool throw_error)
    {
        auto path_result = detail::check_path(config_file.c_str());
        if (path_result == detail::path_type::file)
        {
            try
            {
                std::vector<ConfigItem> values = config_formatter_->from_file(config_file);
                _parse_config(values);
                return true;
            }
            catch (const FileError &)
            {
                if (throw_error)
                {
                    throw;
                }
                return false;
            }
        }
        else if (throw_error)
        {
            throw FileError::Missing(config_file);
        }
        else
        {
            return false;
        }
    }

    void App::_process_config_file()
    {
        if (config_ptr_ != nullptr)
        {
            bool config_required = config_ptr_->get_required();
            auto file_given = config_ptr_->count() > 0;
            if (!(file_given || config_ptr_->envname_.empty()))
            {
                std::string ename_string = detail::get_environment_value(config_ptr_->envname_);
                if (!ename_string.empty())
                {
                    config_ptr_->add_result(ename_string);
                }
            }
            config_ptr_->run_callback();

            auto config_files = config_ptr_->as<std::vector<std::string>>();
            bool files_used {file_given};
            if (config_files.empty() || config_files.front().empty())
            {
                if (config_required)
                {
                    throw FileError("config file is required but none was given");
                }
                return;
            }
            for (const auto &config_file : config_files)
            {
                if (_process_config_file(config_file, config_required || file_given))
                {
                    files_used = true;
                }
            }
            if (!files_used)
            {
                // this is done so the count shows as 0 if no callbacks were processed
                config_ptr_->clear();
                bool force = config_ptr_->force_callback_;
                config_ptr_->force_callback_ = false;
                config_ptr_->run_callback();
                config_ptr_->force_callback_ = force;
            }
        }
    }

    void App::_process_env()
    {
        for (const Option_p &opt : options_)
        {
            if (opt->count() == 0 && !opt->envname_.empty())
            {
                std::string ename_string = detail::get_environment_value(opt->envname_);
                if (!ename_string.empty())
                {
                    std::string result = ename_string;
                    result = opt->_validate(result, 0);
                    if (result.empty())
                    {
                        opt->add_result(ename_string);
                    }
                }
            }
        }

        for (App_p &sub : subcommands_)
        {
            if (sub->get_name().empty() || (sub->count_all() > 0 && !sub->parse_complete_callback_))
            {
                // only process environment variables if the callback has actually been triggered already
                sub->_process_env();
            }
        }
    }

    void App::_process_callbacks(CallbackPriority priority)
    {

        for (App_p &sub : subcommands_)
        {
            // process the priority option_groups first
            if (sub->get_name().empty() && sub->parse_complete_callback_)
            {
                if (sub->count_all() > 0)
                {
                    sub->_process_callbacks(priority);
                    if (priority == CallbackPriority::Normal)
                    {
                        // only run the subcommand callback at normal priority
                        sub->run_callback();
                    }
                }
            }
        }

        for (const Option_p &opt : options_)
        {
            if (opt->get_callback_priority() == priority)
            {
                if ((*opt) && !opt->get_callback_run())
                {
                    opt->run_callback();
                }
            }
        }
        for (App_p &sub : subcommands_)
        {
            if (!sub->parse_complete_callback_)
            {
                sub->_process_callbacks(priority);
            }
        }
    }

    void App::_process_help_flags(CallbackPriority priority, bool trigger_help, bool trigger_all_help) const
    {
        const Option *help_ptr = get_help_ptr();
        const Option *help_all_ptr = get_help_all_ptr();

        if (help_ptr != nullptr && help_ptr->count() > 0 && help_ptr->get_callback_priority() == priority)
        {
            trigger_help = true;
        }
        if (help_all_ptr != nullptr && help_all_ptr->count() > 0 && help_all_ptr->get_callback_priority() == priority)
        {
            trigger_all_help = true;
        }

        // If there were parsed subcommands, call those. First subcommand wins if there are multiple ones.
        if (!parsed_subcommands_.empty())
        {
            for (const App *sub : parsed_subcommands_)
            {
                sub->_process_help_flags(priority, trigger_help, trigger_all_help);
            }

            // Only the final subcommand should call for help. All help wins over help.
        }
        else if (trigger_all_help)
        {
            throw CallForAllHelp();
        }
        else if (trigger_help)
        {
            throw CallForHelp();
        }
    }

    void App::_process_requirements()
    {
        // check excludes
        bool excluded {false};
        std::string excluder;
        for (const auto &opt : exclude_options_)
        {
            if (opt->count() > 0)
            {
                excluded = true;
                excluder = opt->get_name();
            }
        }
        for (const auto &subc : exclude_subcommands_)
        {
            if (subc->count_all() > 0)
            {
                excluded = true;
                excluder = subc->get_display_name();
            }
        }
        if (excluded)
        {
            if (count_all() > 0)
            {
                throw ExcludesError(get_display_name(), excluder);
            }
            // if we are excluded but didn't receive anything, just return
            return;
        }

        // check excludes
        bool missing_needed {false};
        std::string missing_need;
        for (const auto &opt : need_options_)
        {
            if (opt->count() == 0)
            {
                missing_needed = true;
                missing_need = opt->get_name();
            }
        }
        for (const auto &subc : need_subcommands_)
        {
            if (subc->count_all() == 0)
            {
                missing_needed = true;
                missing_need = subc->get_display_name();
            }
        }
        if (missing_needed)
        {
            if (count_all() > 0)
            {
                throw RequiresError(get_display_name(), missing_need);
            }
            // if we missing something but didn't have any options, just return
            return;
        }

        std::size_t used_options = 0;
        for (const Option_p &opt : options_)
        {

            if (opt->count() != 0)
            {
                ++used_options;
            }
            // Required but empty
            if (opt->get_required() && opt->count() == 0)
            {
                throw RequiredError(opt->get_name());
            }
            // Requires
            for (const Option *opt_req : opt->needs_)
                if (opt->count() > 0 && opt_req->count() == 0)
                    throw RequiresError(opt->get_name(), opt_req->get_name());
            // Excludes
            for (const Option *opt_ex : opt->excludes_)
                if (opt->count() > 0 && opt_ex->count() != 0)
                    throw ExcludesError(opt->get_name(), opt_ex->get_name());
        }
        // check for the required number of subcommands
        if (require_subcommand_min_ > 0)
        {
            auto selected_subcommands = get_subcommands();
            if (require_subcommand_min_ > selected_subcommands.size())
                throw RequiredError::Subcommand(require_subcommand_min_);
        }

        // Max error cannot occur, the extra subcommand will parse as an ExtrasError or a remaining item.

        // run this loop to check how many unnamed subcommands were actually used since they are considered options
        // from the perspective of an App
        for (App_p &sub : subcommands_)
        {
            if (sub->disabled_)
                continue;
            if (sub->name_.empty() && sub->count_all() > 0)
            {
                ++used_options;
            }
        }

        if (require_option_min_ > used_options || (require_option_max_ > 0 && require_option_max_ < used_options))
        {
            auto option_list = detail::join(options_, [this](const Option_p &ptr) {
                if (ptr.get() == help_ptr_ || ptr.get() == help_all_ptr_)
                {
                    return std::string {};
                }
                return ptr->get_name(false, true);
            });

            auto subc_list = get_subcommands([](App *app) { return ((app->get_name().empty()) && (!app->disabled_)); });
            if (!subc_list.empty())
            {
                option_list += "," + detail::join(subc_list, [](const App *app) { return app->get_display_name(); });
            }
            throw RequiredError::Option(require_option_min_, require_option_max_, used_options, option_list);
        }

        // now process the requirements for subcommands if needed
        for (App_p &sub : subcommands_)
        {
            if (sub->disabled_)
                continue;
            if (sub->name_.empty() && sub->required_ == false)
            {
                if (sub->count_all() == 0)
                {
                    if (require_option_min_ > 0 && require_option_min_ <= used_options)
                    {
                        continue;
                        // if we have met the requirement and there is nothing in this option group skip checking
                        // requirements
                    }
                    if (require_option_max_ > 0 && used_options >= require_option_min_)
                    {
                        continue;
                        // if we have met the requirement and there is nothing in this option group skip checking
                        // requirements
                    }
                }
            }
            if (sub->count() > 0 || sub->name_.empty())
            {
                sub->_process_requirements();
            }

            if (sub->required_ && sub->count_all() == 0)
            {
                throw(CLI::RequiredError(sub->get_display_name()));
            }
        }
    }

    void App::_process()
    {
        // help takes precedence over other potential errors and config and environment shouldn't be processed if help
        // throws
        _process_callbacks(CallbackPriority::FirstPreHelp);
        _process_help_flags(CallbackPriority::First);
        _process_callbacks(CallbackPriority::First);

        std::exception_ptr config_exception;
        try
        {
            // the config file might generate a FileError but that should not be processed until later in the process
            // to allow for help, version and other errors to generate first.
            _process_config_file();

            // process env shouldn't throw but no reason to process it if config generated an error
            _process_env();
        }
        catch (const CLI::FileError &)
        {
            config_exception = std::current_exception();
        }
        // callbacks and requirements processing can generate exceptions which should take priority
        // over the config file error if one exists.
        _process_callbacks(CallbackPriority::PreRequirementsCheckPreHelp);
        _process_help_flags(CallbackPriority::PreRequirementsCheck);
        _process_callbacks(CallbackPriority::PreRequirementsCheck);

        _process_requirements();

        _process_callbacks(CallbackPriority::NormalPreHelp);
        _process_help_flags(CallbackPriority::Normal);
        _process_callbacks(CallbackPriority::Normal);

        if (config_exception)
        {
            std::rethrow_exception(config_exception);
        }

        _process_callbacks(CallbackPriority::LastPreHelp);
        _process_help_flags(CallbackPriority::Last);
        _process_callbacks(CallbackPriority::Last);
    }

    void App::_process_extras()
    {
        if (allow_extras_ == ExtrasMode::Error && prefix_command_ == PrefixCommandMode::Off)
        {
            std::size_t num_left_over = remaining_size();
            if (num_left_over > 0)
            {
                throw ExtrasError(name_, remaining(false));
            }
        }
        if (allow_extras_ == ExtrasMode::Error && prefix_command_ == PrefixCommandMode::SeparatorOnly)
        {
            std::size_t num_left_over = remaining_size();
            if (num_left_over > 0)
            {
                if (remaining(false).front() != "--")
                {
                    throw ExtrasError(name_, remaining(false));
                }
            }
        }
        for (App_p &sub : subcommands_)
        {
            if (sub->count() > 0)
                sub->_process_extras();
        }
    }

    void App::increment_parsed()
    {
        ++parsed_;
        for (App_p &sub : subcommands_)
        {
            if (sub->get_name().empty())
                sub->increment_parsed();
        }
    }

    void App::_parse(std::vector<std::string> &args)
    {
        increment_parsed();
        _trigger_pre_parse(args.size());
        bool positional_only = false;

        while (!args.empty())
        {
            if (!_parse_single(args, positional_only))
            {
                break;
            }
        }

        if (parent_ == nullptr)
        {
            _process();

            // Throw error if any items are left over (depending on settings)
            _process_extras();
            // Convert missing (pairs) to extras (string only) ready for processing in another app
            args = remaining_for_passthrough(false);
        }
        else if (parse_complete_callback_)
        {
            _process_callbacks(CallbackPriority::FirstPreHelp);
            _process_help_flags(CallbackPriority::First);
            _process_callbacks(CallbackPriority::First);
            _process_env();
            _process_callbacks(CallbackPriority::PreRequirementsCheckPreHelp);
            _process_help_flags(CallbackPriority::PreRequirementsCheck);
            _process_callbacks(CallbackPriority::PreRequirementsCheck);
            _process_requirements();
            _process_callbacks(CallbackPriority::NormalPreHelp);
            _process_help_flags(CallbackPriority::Normal);
            _process_callbacks(CallbackPriority::Normal);
            _process_callbacks(CallbackPriority::LastPreHelp);
            _process_help_flags(CallbackPriority::Last);
            _process_callbacks(CallbackPriority::Last);
            run_callback(false, true);
        }
    }

    void App::_parse(std::vector<std::string> &&args)
    {
        // this can only be called by the top level in which case parent == nullptr by definition
        // operation is simplified
        increment_parsed();
        _trigger_pre_parse(args.size());
        bool positional_only = false;

        while (!args.empty())
        {
            _parse_single(args, positional_only);
        }
        _process();

        // Throw error if any items are left over (depending on settings)
        _process_extras();
    }

    void App::_parse_stream(std::istream &input)
    {
        auto values = config_formatter_->from_config(input);
        _parse_config(values);
        increment_parsed();
        _trigger_pre_parse(values.size());
        _process();

        // Throw error if any items are left over (depending on settings)
        _process_extras();
    }

    void App::_parse_config(const std::vector<ConfigItem> &args)
    {
        for (const ConfigItem &item : args)
        {
            if (!_parse_single_config(item) && allow_config_extras_ == ConfigExtrasMode::Error)
                throw ConfigError::Extras(item.fullname());
        }
    }

    bool App::_add_flag_like_result(Option *op, const ConfigItem &item, const std::vector<std::string> &inputs)
    {
        if (item.inputs.size() <= 1)
        {
            // Flag parsing
            auto res = config_formatter_->to_flag(item);
            bool converted {false};
            if (op->get_disable_flag_override())
            {
                auto val = detail::to_flag_value(res);
                if (val == 1)
                {
                    res = op->get_flag_value(item.name, "{}");
                    converted = true;
                }
            }

            if (!converted)
            {
                errno = 0;
                if (res != "{}" || op->get_expected_max() <= 1)
                {
                    res = op->get_flag_value(item.name, res);
                }
            }

            op->add_result(res);
            return true;
        }
        if (static_cast<int>(inputs.size()) > op->get_items_expected_max() &&
            op->get_multi_option_policy() != MultiOptionPolicy::TakeAll &&
            op->get_multi_option_policy() != MultiOptionPolicy::Join)
        {
            if (op->get_items_expected_max() > 1)
            {
                throw ArgumentMismatch::AtMost(item.fullname(), op->get_items_expected_max(), inputs.size());
            }

            if (!op->get_disable_flag_override())
            {
                throw ConversionError::TooManyInputsFlag(item.fullname());
            }
            // if the disable flag override is set then we must have the flag values match a known flag value
            // this is true regardless of the output value, so an array input is possible and must be accounted for
            for (const auto &res : inputs)
            {
                bool valid_value {false};
                if (op->default_flag_values_.empty())
                {
                    if (res == "true" || res == "false" || res == "1" || res == "0")
                    {
                        valid_value = true;
                    }
                }
                else
                {
                    for (const auto &valid_res : op->default_flag_values_)
                    {
                        if (valid_res.second == res)
                        {
                            valid_value = true;
                            break;
                        }
                    }
                }

                if (valid_value)
                {
                    op->add_result(res);
                }
                else
                {
                    throw InvalidError("invalid flag argument given");
                }
            }
            return true;
        }
        return false;
    }

    bool App::_parse_single_config(const ConfigItem &item, std::size_t level)
    {

        if (level < item.parents.size())
        {
            auto *subcom = get_subcommand_no_throw(item.parents.at(level));
            return (subcom != nullptr) ? subcom->_parse_single_config(item, level + 1) : false;
        }
        // check for section open
        if (item.name == "++")
        {
            if (configurable_)
            {
                increment_parsed();
                _trigger_pre_parse(2);
                if (parent_ != nullptr)
                {
                    parent_->parsed_subcommands_.push_back(this);
                }
            }
            return true;
        }
        // check for section close
        if (item.name == "--")
        {
            if (configurable_ && parse_complete_callback_)
            {
                _process_callbacks(CallbackPriority::FirstPreHelp);
                _process_callbacks(CallbackPriority::First);
                _process_callbacks(CallbackPriority::PreRequirementsCheckPreHelp);
                _process_callbacks(CallbackPriority::PreRequirementsCheck);
                _process_requirements();
                _process_callbacks(CallbackPriority::NormalPreHelp);
                _process_callbacks(CallbackPriority::Normal);
                _process_callbacks(CallbackPriority::LastPreHelp);
                _process_callbacks(CallbackPriority::Last);
                run_callback();
            }
            return true;
        }
        Option *op = get_option_no_throw("--" + item.name);
        if (op == nullptr)
        {
            if (item.name.size() == 1)
            {
                op = get_option_no_throw("-" + item.name);
            }
            if (op == nullptr)
            {
                op = get_option_no_throw(item.name);
            }
        }
        else if (!op->get_configurable())
        {
            if (item.name.size() == 1)
            {
                auto *testop = get_option_no_throw("-" + item.name);
                if (testop != nullptr && testop->get_configurable())
                {
                    op = testop;
                }
            }
        }
        if (op == nullptr || !op->get_configurable())
        {
            std::string iname = item.name;
            auto options = get_options([iname](const CLI::Option *opt) {
                return (opt->get_configurable() &&
                        (opt->check_name(iname) || opt->check_lname(iname) || opt->check_sname(iname)));
            });
            if (!options.empty())
            {
                op = options[0];
            }
        }
        if (op == nullptr)
        {
            // If the option was not present
            if (get_allow_config_extras() == config_extras_mode::capture)
            {
                // Should we worry about classifying the extras properly?
                missing_.emplace_back(detail::Classifier::NONE, item.fullname());
                for (const auto &input : item.inputs)
                {
                    missing_.emplace_back(detail::Classifier::NONE, input);
                }
            }
            return false;
        }

        if (!op->get_configurable())
        {
            if (get_allow_config_extras() == config_extras_mode::ignore_all)
            {
                return false;
            }
            throw ConfigError::NotConfigurable(item.fullname());
        }
        if (op->empty())
        {
            std::vector<std::string> buffer; // a buffer to use for copying and modifying inputs in a few cases
            bool useBuffer {false};
            if (item.multiline)
            {
                if (!op->get_inject_separator())
                {
                    buffer = item.inputs;
                    buffer.erase(std::remove(buffer.begin(), buffer.end(), "%%"), buffer.end());
                    useBuffer = true;
                }
            }
            const std::vector<std::string> &inputs = (useBuffer) ? buffer : item.inputs;
            if (op->get_expected_min() == 0)
            {
                if (_add_flag_like_result(op, item, inputs))
                {
                    return true;
                }
            }
            op->add_result(inputs);
            op->run_callback();
        }

        return true;
    }

    bool App::_parse_single(std::vector<std::string> &args, bool &positional_only)
    {
        bool retval = true;
        detail::Classifier classifier = positional_only ? detail::Classifier::NONE : _recognize(args.back());
        switch (classifier)
        {
        case detail::Classifier::POSITIONAL_MARK:
            args.pop_back();
            positional_only = true;
            if (get_prefix_command())
            {
                // don't care about extras mode here
                missing_.emplace_back(classifier, "--");
                while (!args.empty())
                {
                    missing_.emplace_back(detail::Classifier::NONE, args.back());
                    args.pop_back();
                }
            }
            else if ((!_has_remaining_positionals()) && (parent_ != nullptr))
            {
                retval = false;
            }
            else
            {
                _move_to_missing(classifier, "--");
            }
            break;
        case detail::Classifier::SUBCOMMAND_TERMINATOR:
            // treat this like a positional mark if in the parent app
            args.pop_back();
            retval = false;
            break;
        case detail::Classifier::SUBCOMMAND:
            retval = _parse_subcommand(args);
            break;
        case detail::Classifier::LONG:
        case detail::Classifier::SHORT:
        case detail::Classifier::WINDOWS_STYLE:
            // If already parsed a subcommand, don't accept options_
            retval = _parse_arg(args, classifier, false);
            break;
        case detail::Classifier::NONE:
            // Probably a positional or something for a parent (sub)command
            retval = _parse_positional(args, false);
            if (retval && positionals_at_end_)
            {
                positional_only = true;
            }
            break;
            // LCOV_EXCL_START
        default:
            throw HorribleError("unrecognized classifier (you should not see this!)");
            // LCOV_EXCL_STOP
        }
        return retval;
    }

    [[nodiscard]] std::size_t App::_count_remaining_positionals(bool required_only) const
    {
        std::size_t retval = 0;
        for (const Option_p &opt : options_)
        {
            if (opt->get_positional() && (!required_only || opt->get_required()))
            {
                if (opt->get_items_expected_min() > 0 && static_cast<int>(opt->count()) < opt->get_items_expected_min())
                {
                    retval += static_cast<std::size_t>(opt->get_items_expected_min()) - opt->count();
                }
            }
        }
        return retval;
    }

    [[nodiscard]] bool App::_has_remaining_positionals() const
    {
        for (const Option_p &opt : options_)
        {
            if (opt->get_positional() && ((static_cast<int>(opt->count()) < opt->get_items_expected_min())))
            {
                return true;
            }
        }

        return false;
    }

    bool App::_parse_positional(std::vector<std::string> &args, bool haltOnSubcommand)
    {

        const std::string &positional = args.back();
        Option *posOpt {nullptr};

        if (positionals_at_end_)
        {
            // deal with the case of required arguments at the end which should take precedence over other arguments
            auto arg_rem = args.size();
            auto remreq = _count_remaining_positionals(true);
            if (arg_rem <= remreq)
            {
                for (const Option_p &opt : options_)
                {
                    if (opt->get_positional() && opt->required_)
                    {
                        if (static_cast<int>(opt->count()) < opt->get_items_expected_min())
                        {
                            if (validate_positionals_)
                            {
                                std::string pos = positional;
                                pos = opt->_validate(pos, 0);
                                if (!pos.empty())
                                {
                                    continue;
                                }
                            }
                            posOpt = opt.get();
                            break;
                        }
                    }
                }
            }
        }
        if (posOpt == nullptr)
        {
            for (const Option_p &opt : options_)
            {
                // Eat options, one by one, until done
                if (opt->get_positional() &&
                    (static_cast<int>(opt->count()) < opt->get_items_expected_max() || opt->get_allow_extra_args()))
                {
                    if (validate_positionals_)
                    {
                        std::string pos = positional;
                        pos = opt->_validate(pos, 0);
                        if (!pos.empty())
                        {
                            continue;
                        }
                    }
                    posOpt = opt.get();
                    break;
                }
            }
        }
        if (posOpt != nullptr)
        {
            parse_order_.push_back(posOpt);
            if (posOpt->get_inject_separator())
            {
                if (!posOpt->results().empty() && !posOpt->results().back().empty())
                {
                    posOpt->add_result(std::string {});
                }
            }
            results_t prev;
            if (posOpt->get_trigger_on_parse() && posOpt->current_option_state_ == Option::option_state::callback_run)
            {
                prev = posOpt->results();
                posOpt->clear();
            }
            if (posOpt->get_expected_min() == 0)
            {
                ConfigItem item;
                item.name = posOpt->pname_;
                item.inputs.push_back(positional);
                // input is singular guaranteed to return true in that case
                _add_flag_like_result(posOpt, item, item.inputs);
            }
            else
            {
                posOpt->add_result(positional);
            }

            if (posOpt->get_trigger_on_parse())
            {
                if (!posOpt->empty())
                {
                    posOpt->run_callback();
                }
                else
                {
                    if (!prev.empty())
                    {
                        posOpt->add_result(prev);
                    }
                }
            }

            args.pop_back();
            return true;
        }

        for (auto &subc : subcommands_)
        {
            if ((subc->name_.empty()) && (!subc->disabled_))
            {
                if (subc->_parse_positional(args, false))
                {
                    if (!subc->pre_parse_called_)
                    {
                        subc->_trigger_pre_parse(args.size());
                    }
                    return true;
                }
            }
        }
        // let the parent deal with it if possible
        if (parent_ != nullptr && fallthrough_)
        {
            return _get_fallthrough_parent()->_parse_positional(args, static_cast<bool>(parse_complete_callback_));
        }
        /// Try to find a local subcommand that is repeated
        auto *com = _find_subcommand(args.back(), true, false);
        if (com != nullptr && (require_subcommand_max_ == 0 || require_subcommand_max_ > parsed_subcommands_.size()))
        {
            if (haltOnSubcommand)
            {
                return false;
            }
            args.pop_back();
            com->_parse(args);
            return true;
        }
        if (subcommand_fallthrough_)
        {
            /// now try one last gasp at subcommands that have been executed before, go to root app and try to find a
            /// subcommand in a broader way, if one exists let the parent deal with it
            auto *parent_app = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
            com = parent_app->_find_subcommand(args.back(), true, false);
            if (com != nullptr && (com->parent_->require_subcommand_max_ == 0 ||
                                   com->parent_->require_subcommand_max_ > com->parent_->parsed_subcommands_.size()))
            {
                return false;
            }
        }
        if (positionals_at_end_)
        {
            std::vector<std::string> rargs;
            rargs.resize(args.size());
            std::reverse_copy(args.begin(), args.end(), rargs.begin());
            throw CLI::ExtrasError(name_, rargs);
        }
        /// If this is an option group don't deal with it
        if (parent_ != nullptr && name_.empty())
        {
            return false;
        }
        /// We are out of other options this goes to missing
        _move_to_missing(detail::Classifier::NONE, positional);
        args.pop_back();
        if (get_prefix_command())
        {
            while (!args.empty())
            {
                missing_.emplace_back(detail::Classifier::NONE, args.back());
                args.pop_back();
            }
        }

        return true;
    }

    [[nodiscard]] App *App::_find_subcommand(const std::string &subc_name,
                                             bool ignore_disabled,
                                             bool ignore_used) const noexcept
    {
        App *bcom {nullptr};
        for (const App_p &com : subcommands_)
        {
            if (com->disabled_ && ignore_disabled)
                continue;
            if (com->get_name().empty())
            {
                auto *subc = com->_find_subcommand(subc_name, ignore_disabled, ignore_used);
                if (subc != nullptr)
                {
                    if (bcom != nullptr)
                    {
                        return nullptr;
                    }
                    bcom = subc;
                    if (!allow_prefix_matching_)
                    {
                        return bcom;
                    }
                }
            }
            auto res = com->check_name_detail(subc_name);
            if (res != NameMatch::none)
            {
                if ((!*com) || !ignore_used)
                {
                    if (res == NameMatch::exact)
                    {
                        return com.get();
                    }
                    if (bcom != nullptr)
                    {
                        return nullptr;
                    }
                    bcom = com.get();
                    if (!allow_prefix_matching_)
                    {
                        return bcom;
                    }
                }
            }
        }
        return bcom;
    }

    bool App::_parse_subcommand(std::vector<std::string> &args)
    {
        if (_count_remaining_positionals(/* required */ true) > 0)
        {
            _parse_positional(args, false);
            return true;
        }
        auto *com = _find_subcommand(args.back(), true, true);
        if (com == nullptr)
        {
            // the main way to get here is using .notation
            auto dotloc = args.back().find_first_of('.');
            if (dotloc != std::string::npos)
            {
                com = _find_subcommand(args.back().substr(0, dotloc), true, true);
                if (com != nullptr)
                {
                    args.back() = args.back().substr(dotloc + 1);
                    args.push_back(com->get_display_name());
                }
            }
        }
        if (com != nullptr)
        {
            args.pop_back();
            if (!com->silent_)
            {
                parsed_subcommands_.push_back(com);
            }
            com->_parse(args);
            auto *parent_app = com->parent_;
            while (parent_app != this)
            {
                parent_app->_trigger_pre_parse(args.size());
                if (!com->silent_)
                {
                    parent_app->parsed_subcommands_.push_back(com);
                }
                parent_app = parent_app->parent_;
            }
            return true;
        }

        if (parent_ == nullptr)
            throw HorribleError("Subcommand " + args.back() + " missing");
        return false;
    }

    bool App::_parse_arg(std::vector<std::string> &args, detail::Classifier current_type, bool local_processing_only)
    {

        std::string current = args.back();

        std::string arg_name;
        std::string value;
        std::string rest;

        switch (current_type)
        {
        case detail::Classifier::LONG:
            if (!detail::split_long(current, arg_name, value))
                throw HorribleError("Long parsed but missing (you should not see this):" + args.back());
            break;
        case detail::Classifier::SHORT:
            if (!detail::split_short(current, arg_name, rest))
                throw HorribleError("Short parsed but missing! You should not see this");
            break;
        case detail::Classifier::WINDOWS_STYLE:
            if (!detail::split_windows_style(current, arg_name, value))
                throw HorribleError("windows option parsed but missing! You should not see this");
            break;
        case detail::Classifier::SUBCOMMAND:
        case detail::Classifier::SUBCOMMAND_TERMINATOR:
        case detail::Classifier::POSITIONAL_MARK:
        case detail::Classifier::NONE:
        default:
            throw HorribleError("parsing got called with invalid option! You should not see this");
        }

        auto op_ptr =
            std::find_if(std::begin(options_), std::end(options_), [arg_name, current_type](const Option_p &opt) {
                if (current_type == detail::Classifier::LONG)
                    return opt->check_lname(arg_name);
                if (current_type == detail::Classifier::SHORT)
                    return opt->check_sname(arg_name);
                // this will only get called for detail::Classifier::WINDOWS_STYLE
                return opt->check_lname(arg_name) || opt->check_sname(arg_name);
            });

        // Option not found
        while (op_ptr == std::end(options_))
        {
            // using while so we can break
            for (auto &subc : subcommands_)
            {
                if (subc->name_.empty() && !subc->disabled_)
                {
                    if (subc->_parse_arg(args, current_type, local_processing_only))
                    {
                        if (!subc->pre_parse_called_)
                        {
                            subc->_trigger_pre_parse(args.size());
                        }
                        return true;
                    }
                }
            }
            if (allow_non_standard_options_ && current_type == detail::Classifier::SHORT && current.size() > 2)
            {
                std::string narg_name;
                std::string nvalue;
                detail::split_long(std::string {'-'} + current, narg_name, nvalue);
                op_ptr = std::find_if(std::begin(options_), std::end(options_), [narg_name](const Option_p &opt) {
                    return opt->check_sname(narg_name);
                });
                if (op_ptr != std::end(options_))
                {
                    arg_name = narg_name;
                    value = nvalue;
                    rest.clear();
                    break;
                }
            }

            // don't capture missing if this is a nameless subcommand and nameless subcommands can't fallthrough
            if (parent_ != nullptr && name_.empty())
            {
                return false;
            }

            // now check for '.' notation of subcommands
            auto dotloc = arg_name.find_first_of('.', 1);
            if (dotloc != std::string::npos && dotloc < arg_name.size() - 1)
            {
                // using dot notation is equivalent to single argument subcommand
                auto *sub = _find_subcommand(arg_name.substr(0, dotloc), true, false);
                if (sub != nullptr)
                {
                    std::string v = args.back();
                    args.pop_back();
                    arg_name = arg_name.substr(dotloc + 1);
                    if (arg_name.size() > 1)
                    {
                        args.push_back(std::string("--") + v.substr(dotloc + 3));
                        current_type = detail::Classifier::LONG;
                    }
                    else
                    {
                        auto nval = v.substr(dotloc + 2);
                        nval.front() = '-';
                        if (nval.size() > 2)
                        {
                            // '=' not allowed in short form arguments
                            args.push_back(nval.substr(3));
                            nval.resize(2);
                        }
                        args.push_back(nval);
                        current_type = detail::Classifier::SHORT;
                    }
                    std::string dummy1, dummy2;
                    bool val = false;
                    if ((current_type == detail::Classifier::SHORT && detail::valid_first_char(args.back()[1])) ||
                        detail::split_long(args.back(), dummy1, dummy2))
                    {
                        val = sub->_parse_arg(args, current_type, true);
                    }

                    if (val)
                    {
                        if (!sub->silent_)
                        {
                            parsed_subcommands_.push_back(sub);
                        }
                        // deal with preparsing
                        increment_parsed();
                        _trigger_pre_parse(args.size());
                        // run the parse complete callback since the subcommand processing is now complete
                        if (sub->parse_complete_callback_)
                        {
                            sub->_process_callbacks(CallbackPriority::FirstPreHelp);
                            sub->_process_help_flags(CallbackPriority::First);
                            sub->_process_callbacks(CallbackPriority::First);
                            sub->_process_env();
                            sub->_process_callbacks(CallbackPriority::PreRequirementsCheckPreHelp);
                            sub->_process_help_flags(CallbackPriority::PreRequirementsCheck);
                            sub->_process_callbacks(CallbackPriority::PreRequirementsCheck);
                            sub->_process_requirements();
                            sub->_process_callbacks(CallbackPriority::NormalPreHelp);
                            sub->_process_help_flags(CallbackPriority::Normal);
                            sub->_process_callbacks(CallbackPriority::Normal);
                            sub->_process_callbacks(CallbackPriority::LastPreHelp);
                            sub->_process_help_flags(CallbackPriority::Last);
                            sub->_process_callbacks(CallbackPriority::Last);
                            sub->run_callback(false, true);
                        }
                        return true;
                    }
                    args.pop_back();
                    args.push_back(v);
                }
            }
            if (local_processing_only)
            {
                return false;
            }
            // If a subcommand, try the main command
            if (parent_ != nullptr && fallthrough_)
                return _get_fallthrough_parent()->_parse_arg(args, current_type, false);

            // Otherwise, add to missing
            args.pop_back();
            _move_to_missing(current_type, current);
            if (get_prefix_command_mode() == PrefixCommandMode::On)
            {
                while (!args.empty())
                {
                    missing_.emplace_back(detail::Classifier::NONE, args.back());
                    args.pop_back();
                }
            }
            else if (allow_extras_ == ExtrasMode::AssumeSingleArgument)
            {
                if (!args.empty() && _recognize(args.back(), false) == detail::Classifier::NONE)
                {
                    _move_to_missing(detail::Classifier::NONE, args.back());
                    args.pop_back();
                }
            }
            else if (allow_extras_ == ExtrasMode::AssumeMultipleArguments)
            {
                while (!args.empty() && _recognize(args.back(), false) == detail::Classifier::NONE)
                {
                    _move_to_missing(detail::Classifier::NONE, args.back());
                    args.pop_back();
                }
            }
            return true;
        }

        args.pop_back();

        // Get a reference to the pointer to make syntax bearable
        Option_p &op = *op_ptr;
        /// if we require a separator add it here
        if (op->get_inject_separator())
        {
            if (!op->results().empty() && !op->results().back().empty())
            {
                op->add_result(std::string {});
            }
        }
        if (op->get_trigger_on_parse() && op->current_option_state_ == Option::option_state::callback_run)
        {
            op->clear();
        }
        int min_num = (std::min)(op->get_type_size_min(), op->get_items_expected_min());
        int max_num = op->get_items_expected_max();
        // check container like options to limit the argument size to a single type if the allow_extra_flags argument is
        // set. 16 is somewhat arbitrary (needs to be at least 4)
        if (max_num >= detail::expected_max_vector_size / 16 && !op->get_allow_extra_args())
        {
            auto tmax = op->get_type_size_max();
            max_num = detail::checked_multiply(tmax, op->get_expected_min()) ? tmax : detail::expected_max_vector_size;
        }
        // Make sure we always eat the minimum for unlimited vectors
        int collected = 0;    // total number of arguments collected
        int result_count = 0; // local variable for number of results in a single arg string
        // deal with purely flag like things
        if (max_num == 0)
        {
            auto res = op->get_flag_value(arg_name, value);
            op->add_result(res);
            parse_order_.push_back(op.get());
        }
        else if (!value.empty())
        { // --this=value
            op->add_result(value, result_count);
            parse_order_.push_back(op.get());
            collected += result_count;
            // -Trest
        }
        else if (!rest.empty())
        {
            op->add_result(rest, result_count);
            parse_order_.push_back(op.get());
            rest = "";
            collected += result_count;
        }

        // gather the minimum number of arguments
        while (min_num > collected && !args.empty())
        {
            std::string current_ = args.back();
            args.pop_back();
            op->add_result(current_, result_count);
            parse_order_.push_back(op.get());
            collected += result_count;
        }

        if (min_num > collected)
        { // if we have run out of arguments and the minimum was not met
            throw ArgumentMismatch::TypedAtLeast(op->get_name(), min_num, op->get_type_name());
        }

        // now check for optional arguments
        if (max_num > collected || op->get_allow_extra_args())
        { // we allow optional arguments
            auto remreqpos = _count_remaining_positionals(true);
            // we have met the minimum now optionally check up to the maximum
            while ((collected < max_num || op->get_allow_extra_args()) && !args.empty() &&
                   _recognize(args.back(), false) == detail::Classifier::NONE)
            {
                // If any required positionals remain, don't keep eating
                if (remreqpos >= args.size())
                {
                    break;
                }
                if (validate_optional_arguments_)
                {
                    std::string arg = args.back();
                    arg = op->_validate(arg, 0);
                    if (!arg.empty())
                    {
                        break;
                    }
                }
                op->add_result(args.back(), result_count);
                parse_order_.push_back(op.get());
                args.pop_back();
                collected += result_count;
            }

            // Allow -- to end an unlimited list and "eat" it
            if (!args.empty() && _recognize(args.back()) == detail::Classifier::POSITIONAL_MARK)
                args.pop_back();
            // optional flag that didn't receive anything now get the default value
            if (min_num == 0 && max_num > 0 && collected == 0)
            {
                auto res = op->get_flag_value(arg_name, std::string {});
                op->add_result(res);
                parse_order_.push_back(op.get());
            }
        }
        // if we only partially completed a type then add an empty string if allowed for later processing
        if (min_num > 0 && (collected % op->get_type_size_max()) != 0)
        {
            if (op->get_type_size_max() != op->get_type_size_min())
            {
                op->add_result(std::string {});
            }
            else
            {
                throw ArgumentMismatch::PartialType(op->get_name(), op->get_type_size_min(), op->get_type_name());
            }
        }
        if (op->get_trigger_on_parse())
        {
            op->run_callback();
        }
        if (!rest.empty())
        {
            rest = "-" + rest;
            args.push_back(rest);
        }
        return true;
    }

    void App::_trigger_pre_parse(std::size_t remaining_args)
    {
        if (!pre_parse_called_)
        {
            pre_parse_called_ = true;
            if (pre_parse_callback_)
            {
                pre_parse_callback_(remaining_args);
            }
        }
        else if (immediate_callback_)
        {
            if (!name_.empty())
            {
                auto pcnt = parsed_;
                missing_t extras = std::move(missing_);
                clear();
                parsed_ = pcnt;
                pre_parse_called_ = true;
                missing_ = std::move(extras);
            }
        }
    }

    App *App::_get_fallthrough_parent() noexcept
    {
        if (parent_ == nullptr)
        {
            return nullptr;
        }
        auto *fallthrough_parent = parent_;
        while ((fallthrough_parent->parent_ != nullptr) && (fallthrough_parent->get_name().empty()))
        {
            fallthrough_parent = fallthrough_parent->parent_;
        }
        return fallthrough_parent;
    }

    const App *App::_get_fallthrough_parent() const noexcept
    {
        if (parent_ == nullptr)
        {
            return nullptr;
        }
        const auto *fallthrough_parent = parent_;
        while ((fallthrough_parent->parent_ != nullptr) && (fallthrough_parent->get_name().empty()))
        {
            fallthrough_parent = fallthrough_parent->parent_;
        }
        return fallthrough_parent;
    }

    [[nodiscard]] const std::string &App::_compare_subcommand_names(const App &subcom, const App &base) const
    {
        static const std::string estring;
        if (subcom.disabled_)
        {
            return estring;
        }
        for (const auto &subc : base.subcommands_)
        {
            if (subc.get() != &subcom)
            {
                if (subc->disabled_)
                {
                    continue;
                }
                if (!subcom.get_name().empty())
                {
                    if (subc->check_name(subcom.get_name()))
                    {
                        return subcom.get_name();
                    }
                }
                if (!subc->get_name().empty())
                {
                    if (subcom.check_name(subc->get_name()))
                    {
                        return subc->get_name();
                    }
                }
                for (const auto &les : subcom.aliases_)
                {
                    if (subc->check_name(les))
                    {
                        return les;
                    }
                }
                // this loop is needed in case of ignore_underscore or ignore_case on one but not the other
                for (const auto &les : subc->aliases_)
                {
                    if (subcom.check_name(les))
                    {
                        return les;
                    }
                }
                // if the subcommand is an option group we need to check deeper
                if (subc->get_name().empty())
                {
                    const auto &cmpres = _compare_subcommand_names(subcom, *subc);
                    if (!cmpres.empty())
                    {
                        return cmpres;
                    }
                }
                // if the test subcommand is an option group we need to check deeper
                if (subcom.get_name().empty())
                {
                    const auto &cmpres = _compare_subcommand_names(*subc, subcom);
                    if (!cmpres.empty())
                    {
                        return cmpres;
                    }
                }
            }
        }
        return estring;
    }

    bool capture_extras(ExtrasMode mode)
    {
        return mode == ExtrasMode::Capture || mode == ExtrasMode::AssumeSingleArgument ||
               mode == ExtrasMode::AssumeMultipleArguments;
    }
    void App::_move_to_missing(detail::Classifier val_type, const std::string &val)
    {
        if (allow_extras_ == ExtrasMode::ErrorImmediately)
        {
            throw ExtrasError(name_, std::vector<std::string> {val});
        }
        if (capture_extras(allow_extras_) || subcommands_.empty() || get_prefix_command())
        {
            if (allow_extras_ != ExtrasMode::Ignore)
            {
                missing_.emplace_back(val_type, val);
            }
            return;
        }
        // allow extra arguments to be placed in an option group if it is allowed there
        for (auto &subc : subcommands_)
        {
            if (subc->name_.empty() && capture_extras(subc->allow_extras_))
            {
                subc->missing_.emplace_back(val_type, val);
                return;
            }
        }
        if (allow_extras_ != ExtrasMode::Ignore)
        {
            // if we haven't found any place to put them yet put them in missing
            missing_.emplace_back(val_type, val);
        }
    }

    void App::_move_option(Option *opt, App *app)
    {
        if (opt == nullptr)
        {
            throw OptionNotFound("the option is NULL");
        }
        // verify that the give app is actually a subcommand
        bool found = false;
        for (auto &subc : subcommands_)
        {
            if (app == subc.get())
            {
                found = true;
            }
        }
        if (!found)
        {
            throw OptionNotFound("The Given app is not a subcommand");
        }

        if ((help_ptr_ == opt) || (help_all_ptr_ == opt))
            throw OptionAlreadyAdded("cannot move help options");

        if (config_ptr_ == opt)
            throw OptionAlreadyAdded("cannot move config file options");

        auto iterator =
            std::find_if(std::begin(options_), std::end(options_), [opt](const Option_p &v) { return v.get() == opt; });
        if (iterator != std::end(options_))
        {
            const auto &opt_p = *iterator;
            if (std::find_if(std::begin(app->options_), std::end(app->options_), [&opt_p](const Option_p &v) {
                    return (*v == *opt_p);
                }) == std::end(app->options_))
            {
                // only erase after the insertion was successful
                app->options_.push_back(std::move(*iterator));
                options_.erase(iterator);
            }
            else
            {
                throw OptionAlreadyAdded("option was not located: " + opt->get_name());
            }
        }
        else
        {
            throw OptionNotFound("could not locate the given Option");
        }
    }

    void TriggerOn(App *trigger_app, App *app_to_enable)
    {
        app_to_enable->enabled_by_default(false);
        app_to_enable->disabled_by_default();
        trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(false); });
    }

    void TriggerOn(App *trigger_app, std::vector<App *> apps_to_enable)
    {
        for (auto &app : apps_to_enable)
        {
            app->enabled_by_default(false);
            app->disabled_by_default();
        }

        trigger_app->preparse_callback([apps_to_enable](std::size_t) {
            for (const auto &app : apps_to_enable)
            {
                app->disabled(false);
            }
        });
    }

    void TriggerOff(App *trigger_app, App *app_to_enable)
    {
        app_to_enable->disabled_by_default(false);
        app_to_enable->enabled_by_default();
        trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(); });
    }

    void TriggerOff(App *trigger_app, std::vector<App *> apps_to_enable)
    {
        for (auto &app : apps_to_enable)
        {
            app->disabled_by_default(false);
            app->enabled_by_default();
        }

        trigger_app->preparse_callback([apps_to_enable](std::size_t) {
            for (const auto &app : apps_to_enable)
            {
                app->disabled();
            }
        });
    }

    void deprecate_option(Option *opt, const std::string &replacement)
    {
        Validator deprecate_warning {[opt, replacement](std::string &) {
                                         std::cout << opt->get_name() << " is deprecated please use '" << replacement
                                                   << "' instead\n";
                                         return std::string();
                                     },
                                     "DEPRECATED"};
        deprecate_warning.application_index(0);
        opt->check(deprecate_warning);
        if (!replacement.empty())
        {
            opt->description(opt->get_description() + " DEPRECATED: please use '" + replacement + "' instead");
        }
    }

    void retire_option(App *app, Option *opt)
    {
        App temp;
        auto *option_copy = temp.add_option(opt->get_name(false, true))
                                ->type_size(opt->get_type_size_min(), opt->get_type_size_max())
                                ->expected(opt->get_expected_min(), opt->get_expected_max())
                                ->allow_extra_args(opt->get_allow_extra_args());

        app->remove_option(opt);
        auto *opt2 = app->add_option(option_copy->get_name(false, true), "option has been retired and has no effect");
        opt2->type_name("RETIRED")
            ->default_str("RETIRED")
            ->type_size(option_copy->get_type_size_min(), option_copy->get_type_size_max())
            ->expected(option_copy->get_expected_min(), option_copy->get_expected_max())
            ->allow_extra_args(option_copy->get_allow_extra_args());

        // LCOV_EXCL_START
        // something odd with coverage on new compilers
        Validator retired_warning {[opt2](std::string &) {
                                       std::cout << "WARNING " << opt2->get_name() << " is retired and has no effect\n";
                                       return std::string();
                                   },
                                   ""};
        // LCOV_EXCL_STOP
        retired_warning.application_index(0);
        opt2->check(retired_warning);
    }

    void retire_option(App &app, Option *opt)
    {
        retire_option(&app, opt);
    }

    void retire_option(App *app, const std::string &option_name)
    {

        auto *opt = app->get_option_no_throw(option_name);
        if (opt != nullptr)
        {
            retire_option(app, opt);
            return;
        }
        auto *opt2 = app->add_option(option_name, "option has been retired and has no effect")
                         ->type_name("RETIRED")
                         ->expected(0, 1)
                         ->default_str("RETIRED");
        // LCOV_EXCL_START
        // something odd with coverage on new compilers
        Validator retired_warning {[opt2](std::string &) {
                                       std::cout << "WARNING " << opt2->get_name() << " is retired and has no effect\n";
                                       return std::string();
                                   },
                                   ""};
        // LCOV_EXCL_STOP
        retired_warning.application_index(0);
        opt2->check(retired_warning);
    }

    void retire_option(App &app, const std::string &option_name)
    {
        retire_option(&app, option_name);
    }

    namespace FailureMessage
    {

        std::string simple(const App *app, const Error &e)
        {
            std::string header = std::string(e.what()) + "\n";
            std::vector<std::string> names;

            // Collect names
            if (app->get_help_ptr() != nullptr)
                names.push_back(app->get_help_ptr()->get_name());

            if (app->get_help_all_ptr() != nullptr)
                names.push_back(app->get_help_all_ptr()->get_name());

            // If any names found, suggest those
            if (!names.empty())
                header += "Run with " + detail::join(names, " or ") + " for more information.\n";

            return header;
        }

        std::string help(const App *app, const Error &e)
        {
            std::string header = std::string("ERROR: ") + e.get_name() + ": " + e.what() + "\n";
            header += app->help();
            return header;
        }

    } // namespace FailureMessage

} // namespace CLI
