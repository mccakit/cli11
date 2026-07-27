/// @file
/// @brief The application type: the parser itself.
///
/// @ref cli::app_t is the entry point. Construct one, describe the interface with
/// `add_option`, `add_flag`, and `add_subcommand`, then call `parse`:
///
/// @code
/// int main(int argc, char **argv)
/// {
///     cli::app_t app{"my program"};
///     std::string file;
///     app.add_option("-f,--file", file, "the file to read")->required();
///
///     try
///     {
///         app.parse(argc, argv);
///     }
///     catch (const cli::parse_error_t &e)
///     {
///         return app.exit(e);
///     }
/// }
/// @endcode
///
/// @note Upstream CLI11 offers a `CLI11_PARSE` macro for the try/catch above.
/// It has no equivalent here: macros are not exported across module boundaries,
/// so the explicit form is the only one.
///
/// Subcommands are themselves `app_t` instances, so everything that works on the
/// top-level parser works on a subcommand. Option groups are subcommands with no
/// name, which is why one class covers all three roles.
///
/// Parsing runs in phases: arguments are classified, matched to options or
/// subcommands, collected, then validated and dispatched to callbacks. The phase
/// helpers are the `_`-prefixed private members near the end of the class.

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

export namespace cli
{

    namespace detail
    {

        /// @brief What a single command-line argument looks like.
        ///
        /// @note `short_` and `long_` carry trailing underscores because `short` and
        /// `long` are keywords.
        enum class classifier_t : std::uint8_t
        {
            none,                 ///< Not recognised as any of the forms below.
            positional_mark,      ///< The `--` separator.
            short_,               ///< A short option, such as `-f`.
            long_,                ///< A long option, such as `--file`.
            windows_style,        ///< A Windows-style option, such as `/file`.
            subcommand,           ///< The name of a subcommand.
            subcommand_terminator ///< The `++` subcommand terminator.
        };

        struct app_friend_t;

    } // namespace detail

    class app_t;

    namespace failure_message
    {

        /// @brief Prints a short one-line error message.
        ///
        /// The default since CLI11 1.5.
        ///
        /// @param app The application the error came from.
        /// @param e The error to describe.
        /// @return The message to print.
        auto simple(const app_t *app, const error_t &e) -> std::string;

        /// @brief Prints the full help text alongside the error.
        ///
        /// @param app The application the error came from.
        /// @param e The error to describe.
        /// @return The message to print.
        auto help(const app_t *app, const error_t &e) -> std::string;

    } // namespace failure_message

    /// @brief What to do with command-line arguments that match nothing.
    enum class extras_mode_t : std::uint8_t
    {
        error = 0,                ///< Report an error once parsing finishes.
        error_immediately,        ///< Report an error as soon as one is seen.
        ignore,                   ///< Leave them in the remaining-arguments list.
        assume_single_argument,   ///< Treat the next argument as this option's value.
        assume_multiple_arguments,///< Treat every following argument as this option's values.
        capture                   ///< Collect them for the caller to inspect.
    };

    /// @brief What to do with configuration-file entries that match nothing.
    ///
    /// @note The original had two enumerations for this, `ConfigExtrasMode` and
    /// `config_extras_mode`, with identical members. Lowercasing collapsed them into
    /// one, which is this.
    enum class config_extras_mode_t : std::uint8_t
    {
        error = 0,  ///< Report an error.
        ignore,     ///< Ignore the entry.
        ignore_all, ///< Ignore the entry and everything nested under it.
        capture     ///< Collect it for the caller to inspect.
    };

    /// @brief When an unrecognised argument should stop parsing entirely.
    ///
    /// Prefix-command mode hands everything from that point on to the caller, which
    /// is how a program forwards a trailing argument list to another tool.
    enum class prefix_command_mode_t : std::uint8_t
    {
        off = 0,           ///< Never stop early.
        separator_only = 1, ///< Stop only at a `--`; anything else is an error.
        on = 2              ///< Stop at the first unrecognised argument.
    };

    /// @brief Shared handle to an application or subcommand.
    using app_ptr_t = std::shared_ptr<app_t>;

    namespace detail
    {

        /// @brief Applies the default settings for a flag bound to a non-counting type.
        ///
        /// @param opt The option to configure.
        /// @return @p opt, for chaining.
        template <typename T>
            requires(!std::is_integral_v<T> || (sizeof(T) <= 1U))
        auto default_flag_modifiers(option_t *opt) -> option_t *
        {
            return opt->always_capture_default();
        }

        /// @brief Applies the default settings for a flag bound to a counting type.
        ///
        /// An integer wider than a byte is taken to be a counter, so repeated
        /// appearances sum rather than overwrite.
        ///
        /// @param opt The option to configure.
        /// @return @p opt, for chaining.
        template <typename T>
            requires(std::is_integral_v<T> && (sizeof(T) > 1U))
        auto default_flag_modifiers(option_t *opt) -> option_t *
        {
            return opt->multi_option_policy(multi_option_policy_t::sum)->default_str("0")->force_callback();
        }

    } // namespace detail

    class option_group_t;

    /// @brief A command-line parser, a subcommand, or an option group.
    ///
    /// Create one, add options to it, and call @ref parse. Call @ref exit from a
    /// `catch` block to turn a @ref cli::parse_error_t into a process exit code.
    class app_t
    {
            friend option_t;
            friend detail::app_friend_t;

        protected:
            // Member names end in an underscore, following the Google style guide.

            /// @name Basics
            ///@{

            /// @brief Subcommand name, or program name; taken from the parser when empty.
            std::string name_ {};

            /// @brief Description of this program or subcommand.
            std::string description_ {};

            /// @brief What to do with unmatched command-line arguments. Inheritable.
            extras_mode_t allow_extras_ {extras_mode_t::error};

            /// @brief What to do with unmatched configuration entries. Inheritable.
            config_extras_mode_t allow_config_extras_ {config_extras_mode_t::ignore};

            /// @brief Whether an unrecognised argument stops parsing. Inheritable.
            prefix_command_mode_t prefix_command_ {prefix_command_mode_t::off};

            /// @brief Whether the name was taken from the command line rather than set.
            bool has_automatic_name_ {false};

            /// @brief Whether this subcommand must be used; ignored for the main app.
            bool required_ {false};

            /// @brief Whether this subcommand is disabled; ignored for the main app.
            bool disabled_ {false};

            /// @brief Whether @ref pre_parse_callback_ has already fired.
            bool pre_parse_called_ {false};

            /// @brief Whether the callback runs on parse completion, before help and
            /// configuration files are handled. Inheritable.
            bool immediate_callback_ {false};

            /// @brief Runs before parsing starts, given the argument count.
            std::function<void(std::size_t)> pre_parse_callback_ {};

            /// @brief Runs once parsing has finished.
            std::function<void()> parse_complete_callback_ {};

            /// @brief Runs once all processing has finished.
            std::function<void()> final_callback_ {};

            ///@}
            /// @name Options
            ///@{

            /// @brief Settings newly added options inherit. Inheritable.
            option_defaults_t option_defaults_ {};

            /// @brief The options owned by this application.
            std::vector<option_ptr_t> options_ {};

            ///@}
            /// @name Help
            ///@{

            /// @brief Usage line placed after the description. Inheritable.
            std::string usage_ {};

            /// @brief Generates the usage line, when one is not set directly.
            std::function<std::string()> usage_callback_ {};

            /// @brief Footer placed after all options. Inheritable.
            std::string footer_ {};

            /// @brief Generates the footer, when one is not set directly.
            std::function<std::string()> footer_callback_ {};

            /// @brief The help option, if one was added. Inheritable.
            option_t *help_ptr_ {nullptr};

            /// @brief The expanded-help option, if one was added. Inheritable.
            option_t *help_all_ptr_ {nullptr};

            /// @brief The version option, if one was added.
            option_t *version_ptr_ {nullptr};

            /// @brief Renders help output. Inheritable, and shared by pointer.
            std::shared_ptr<formatter_base_t> formatter_ {std::make_shared<formatter_t>()};

            /// @brief Renders an error into the message printed by @ref exit. Inheritable.
            std::function<std::string(const app_t *, const error_t &)> failure_message_ {
                failure_message::simple};

            ///@}
            /// @name Parsing
            ///@{

            /// @brief Arguments that matched nothing, paired with how they were classified.
            ///
            /// Keeping the classification avoids reclassifying on the way back out. May
            /// contain the `--` separator.
            using missing_t = std::vector<std::pair<detail::classifier_t, std::string>>;

            /// @brief Arguments that matched nothing. Extra detail is stripped on return
            /// from @ref parse.
            missing_t missing_ {};

            /// @brief The options that were used, in the order they appeared.
            std::vector<option_t *> parse_order_ {};

            /// @brief The subcommands that were used, in the order they appeared.
            std::vector<app_t *> parsed_subcommands_ {};

            /// @brief Subcommands that may not be used alongside this one.
            std::set<app_t *> exclude_subcommands_ {};

            /// @brief Options that may not be used alongside this subcommand.
            std::set<option_t *> exclude_options_ {};

            /// @brief Subcommands this one requires. Not mutual: they do not require this one.
            std::set<app_t *> need_subcommands_ {};

            /// @brief Options this subcommand requires. Not mutual.
            std::set<option_t *> need_options_ {};

            ///@}
            /// @name Subcommands
            ///@{

            /// @brief The subcommands owned by this application.
            std::vector<app_ptr_t> subcommands_ {};

            /// @brief Whether subcommand name matching ignores case. Inheritable.
            bool ignore_case_ {false};

            /// @brief Whether subcommand name matching ignores underscores. Inheritable.
            bool ignore_underscore_ {false};

            /// @brief Whether options may fall through to a parent command.
            ///
            /// Lets a parent collect options written after a subcommand name. Inheritable.
            bool fallthrough_ {false};

            /// @brief Whether a later subcommand may be triggered from within this one.
            bool subcommand_fallthrough_ {true};

            /// @brief Whether `/opt` is accepted as an option form.
            ///
            /// Defaults to enabled on Windows only. Inheritable.
            bool allow_windows_style_options_ {
#ifdef _WIN32
                true
#else
                false
#endif
            };

            /// @brief Whether positionals must come after every option. Not inheritable.
            bool positionals_at_end_ {false};

            /// @brief Whether @ref clear leaves this subcommand enabled or disabled.
            enum class startup_mode_t : std::uint8_t
            {
                stable,  ///< Leave the enabled state as it is.
                enabled, ///< Enable at the start of each parse.
                disabled ///< Disable at the start of each parse.
            };

            /// @brief What @ref clear does to this subcommand's enabled state.
            startup_mode_t default_startup {startup_mode_t::stable};

            /// @brief Whether this subcommand can be triggered from a configuration file. Inheritable.
            bool configurable_ {false};

            /// @brief Whether positionals are validated before being assigned. Inheritable.
            bool validate_positionals_ {false};

            /// @brief Whether optional vector arguments are validated before being assigned. Inheritable.
            bool validate_optional_arguments_ {false};

            /// @brief Whether this subcommand is hidden from the subcommand list.
            ///
            /// Useful for a subcommand that only modifies how the parent behaves.
            bool silent_ {false};

            /// @brief Whether non-standard option names such as `-single_dash_flag` are accepted.
            bool allow_non_standard_options_ {false};

            /// @brief Whether a subcommand may be matched by a unique prefix of its name.
            bool allow_prefix_matching_ {false};

            /// @brief How many times this command or subcommand has been parsed.
            std::uint32_t parsed_ {0U};

            /// @brief Smallest number of subcommands that must be used. Not inheritable.
            std::size_t require_subcommand_min_ {0};

            /// @brief Largest number of subcommands accepted; `0` is unlimited. Inheritable.
            ///
            /// Parsing stops once this many have been seen.
            std::size_t require_subcommand_max_ {0};

            /// @brief Smallest number of options that must be used. Not inheritable.
            std::size_t require_option_min_ {0};

            /// @brief Largest number of options accepted; `0` is unlimited. Not inheritable.
            std::size_t require_option_max_ {0};

            /// @brief The parent application, if this is a subcommand.
            app_t *parent_ {nullptr};

            /// @brief The help group this subcommand is listed under. Inheritable.
            std::string group_ {"SUBCOMMANDS"};

            /// @brief Alternative names this subcommand answers to.
            std::vector<std::string> aliases_ {};

            ///@}
            /// @name Config
            ///@{

            /// @brief The option naming a configuration file, if one was added.
            option_t *config_ptr_ {nullptr};

            /// @brief Reads and writes configuration files. Inheritable, shared by pointer.
            std::shared_ptr<config_t> config_formatter_ {std::make_shared<config_toml_t>()};

            ///@}

#ifdef _WIN32
            /// @brief Storage backing the UTF-8 arguments produced by @ref ensure_utf8.
            std::vector<std::string> normalized_argv_ {};

            /// @brief The `char **` handed back by @ref ensure_utf8, pointing into
            /// @ref normalized_argv_.
            std::vector<char *> normalized_argv_view_ {};
#endif

            /// @brief Constructs a subcommand.
            ///
            /// @param app_description The description shown in help output.
            /// @param app_name The subcommand name.
            /// @param parent The owning application.
            app_t(std::string app_description, std::string app_name, app_t *parent);

        public:
            /// @name Basic
            ///@{

            /// @brief Constructs a top-level application.
            ///
            /// A `-h,--help` flag is added automatically; remove it with
            /// @ref set_help_flag if you want to supply your own.
            ///
            /// @param app_description The description shown in help output.
            /// @param app_name The program name; taken from the command line when empty.
            explicit app_t(std::string app_description = "", std::string app_name = "")
                : app_t(std::move(app_description), std::move(app_name), nullptr)
            {
                set_help_flag("-h,--help", "Print this help message and exit");
            }

            app_t(const app_t &) = delete;
            auto operator=(const app_t &) -> app_t & = delete;

            virtual ~app_t() = default;

            /// @brief Converts `argv` to UTF-8 on Windows; does nothing elsewhere.
            ///
            /// The converted strings are owned by this application, so the returned
            /// pointer stays valid for as long as it does.
            ///
            /// @param argv The argument array from `main`.
            /// @return The converted argument array.
            [[nodiscard]] auto ensure_utf8(char **argv) -> char **;

            /// @brief Sets the callback run once everything has been parsed and processed.
            ///
            /// Assigns to @ref parse_complete_callback when @ref immediate_callback is
            /// set, and to @ref final_callback otherwise. Capture by reference if the
            /// callback needs the application itself.
            ///
            /// @param app_callback The callback to run.
            /// @return A pointer to this application, for chaining.
            auto callback(std::function<void()> app_callback) -> app_t *
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

            /// @brief Sets the callback run once everything has been processed.
            ///
            /// @param app_callback The callback to run.
            /// @return A pointer to this application, for chaining.
            auto final_callback(std::function<void()> app_callback) -> app_t *
            {
                final_callback_ = std::move(app_callback);
                return this;
            }

            /// @brief Sets the callback run once parsing has finished.
            ///
            /// @param pc_callback The callback to run.
            /// @return A pointer to this application, for chaining.
            auto parse_complete_callback(std::function<void()> pc_callback) -> app_t *
            {
                parse_complete_callback_ = std::move(pc_callback);
                return this;
            }

            /// @brief Sets the callback run before parsing starts.
            ///
            /// @param pp_callback The callback to run, given the argument count.
            /// @return A pointer to this application, for chaining.
            auto preparse_callback(std::function<void(std::size_t)> pp_callback) -> app_t *
            {
                pre_parse_callback_ = std::move(pp_callback);
                return this;
            }

            /// @brief Sets the application or subcommand name.
            ///
            /// @param app_name The name; an empty string defers to the command line.
            /// @return A pointer to this application, for chaining.
            auto name(std::string app_name = "") -> app_t *;

            /// @brief Adds an alternative name this subcommand answers to.
            ///
            /// @param app_name The alias.
            /// @return A pointer to this application, for chaining.
            auto alias(std::string app_name) -> app_t *;

            /// @brief Accepts unmatched arguments instead of reporting them.
            ///
            /// @param allow Whether to accept them.
            /// @return A pointer to this application, for chaining.
            auto allow_extras(bool allow = true) -> app_t *
            {
                allow_extras_ = allow ? extras_mode_t::capture : extras_mode_t::error;
                return this;
            }

            /// @brief Sets what happens to unmatched arguments.
            ///
            /// @param allow The mode to use.
            /// @return A pointer to this application, for chaining.
            auto allow_extras(extras_mode_t allow) -> app_t *
            {
                allow_extras_ = allow;
                return this;
            }

            /// @brief Requires this subcommand to be used.
            ///
            /// @param require Whether the subcommand is required.
            /// @return A pointer to this application, for chaining.
            auto required(bool require = true) -> app_t *
            {
                required_ = require;
                return this;
            }

            /// @brief Disables this subcommand or option group.
            ///
            /// @param disable Whether to disable it.
            /// @return A pointer to this application, for chaining.
            auto disabled(bool disable = true) -> app_t *
            {
                disabled_ = disable;
                return this;
            }

            /// @brief Hides this subcommand from the processed subcommand list.
            ///
            /// @param silence Whether to hide it.
            /// @return A pointer to this application, for chaining.
            auto silent(bool silence = true) -> app_t *
            {
                silent_ = silence;
                return this;
            }

            /// @brief Accepts non-standard option names such as `-single_dash_flag`.
            ///
            /// @param allowed Whether to accept them.
            /// @return A pointer to this application, for chaining.
            auto allow_non_standard_option_names(bool allowed = true) -> app_t *
            {
                allow_non_standard_options_ = allowed;
                return this;
            }

            /// @brief Lets a subcommand be matched by a unique prefix of its name.
            ///
            /// @param allowed Whether to allow prefix matching.
            /// @return A pointer to this application, for chaining.
            auto allow_subcommand_prefix_matching(bool allowed = true) -> app_t *
            {
                allow_prefix_matching_ = allowed;
                return this;
            }

            /// @brief Makes @ref clear leave this subcommand disabled.
            ///
            /// Turning this off returns to the stable state unless
            /// @ref enabled_by_default was set, which takes precedence.
            ///
            /// @param disable Whether to disable by default.
            /// @return A pointer to this application, for chaining.
            auto disabled_by_default(bool disable = true) -> app_t *
            {
                if (disable)
                {
                    default_startup = startup_mode_t::disabled;
                }
                else
                {
                    default_startup = (default_startup == startup_mode_t::enabled) ? startup_mode_t::enabled
                                                                                   : startup_mode_t::stable;
                }
                return this;
            }

            /// @brief Makes @ref clear leave this subcommand enabled.
            ///
            /// Turning this off returns to the stable state unless
            /// @ref disabled_by_default was set, which takes precedence.
            ///
            /// @param enable Whether to enable by default.
            /// @return A pointer to this application, for chaining.
            auto enabled_by_default(bool enable = true) -> app_t *
            {
                if (enable)
                {
                    default_startup = startup_mode_t::enabled;
                }
                else
                {
                    default_startup = (default_startup == startup_mode_t::disabled) ? startup_mode_t::disabled
                                                                                    : startup_mode_t::stable;
                }
                return this;
            }

            /// @brief Runs this subcommand's callback as soon as it finishes parsing.
            ///
            /// @param immediate Whether to run the callback immediately.
            /// @return A pointer to this application, for chaining.
            auto immediate_callback(bool immediate = true) -> app_t *;

            /// @brief Validates positionals before assigning them.
            ///
            /// @param validate Whether to validate first.
            /// @return A pointer to this application, for chaining.
            auto validate_positionals(bool validate = true) -> app_t *
            {
                validate_positionals_ = validate;
                return this;
            }

            /// @brief Validates optional vector arguments before assigning them.
            ///
            /// @param validate Whether to validate first.
            /// @return A pointer to this application, for chaining.
            auto validate_optional_arguments(bool validate = true) -> app_t *
            {
                validate_optional_arguments_ = validate;
                return this;
            }

            /// @brief Accepts unmatched configuration entries instead of reporting them.
            ///
            /// Enabling this also enables @ref allow_extras, since a captured
            /// configuration entry has to go somewhere.
            ///
            /// @param allow Whether to accept them.
            /// @return A pointer to this application, for chaining.
            auto allow_config_extras(bool allow = true) -> app_t *
            {
                if (allow)
                {
                    allow_config_extras_ = config_extras_mode_t::capture;
                    allow_extras_ = extras_mode_t::capture;
                }
                else
                {
                    allow_config_extras_ = config_extras_mode_t::error;
                }
                return this;
            }

            /// @brief Sets what happens to unmatched configuration entries.
            ///
            /// @note The original declared this twice, once taking `config_extras_mode`
            /// and once taking `ConfigExtrasMode`. Those were distinct types holding the
            /// same enumerators, and merging them leaves a single overload.
            ///
            /// @param mode The mode to use.
            /// @return A pointer to this application, for chaining.
            auto allow_config_extras(config_extras_mode_t mode) -> app_t *
            {
                allow_config_extras_ = mode;
                return this;
            }

            /// @brief Stops parsing at the first unrecognised argument.
            ///
            /// Everything from that point on is left in the remaining-arguments list.
            ///
            /// @param is_prefix Whether to stop early.
            /// @return A pointer to this application, for chaining.
            auto prefix_command(bool is_prefix = true) -> app_t *
            {
                prefix_command_ = is_prefix ? prefix_command_mode_t::on : prefix_command_mode_t::off;
                return this;
            }

            /// @brief Sets when parsing stops early.
            ///
            /// @param mode The mode to use.
            /// @return A pointer to this application, for chaining.
            auto prefix_command(prefix_command_mode_t mode) -> app_t *
            {
                prefix_command_ = mode;
                return this;
            }

            /// @brief Makes name matching case-insensitive. Subcommands inherit this.
            ///
            /// @param value Whether to ignore case.
            /// @return A pointer to this application, for chaining.
            auto ignore_case(bool value = true) -> app_t *;

            /// @brief Accepts Windows-style options such as `/opt`.
            ///
            /// The first matching short or long name wins. Subcommands inherit this.
            ///
            /// @param value Whether to accept them.
            /// @return A pointer to this application, for chaining.
            auto allow_windows_style_options(bool value = true) -> app_t *
            {
                allow_windows_style_options_ = value;
                return this;
            }

            /// @brief Requires positionals to appear after every option.
            ///
            /// @param value Whether positionals come last.
            /// @return A pointer to this application, for chaining.
            auto positionals_at_end(bool value = true) -> app_t *
            {
                positionals_at_end_ = value;
                return this;
            }

            /// @brief Lets this subcommand be triggered from a configuration file.
            ///
            /// @param value Whether the subcommand is configurable.
            /// @return A pointer to this application, for chaining.
            auto configurable(bool value = true) -> app_t *
            {
                configurable_ = value;
                return this;
            }

            /// @brief Makes name matching ignore underscores. Subcommands inherit this.
            ///
            /// @param value Whether to ignore underscores.
            /// @return A pointer to this application, for chaining.
            auto ignore_underscore(bool value = true) -> app_t *;

            /// @brief Replaces the help formatter.
            ///
            /// @param fmt The formatter to use.
            /// @return A pointer to this application, for chaining.
            auto formatter(std::shared_ptr<formatter_base_t> fmt) -> app_t *
            {
                formatter_ = std::move(fmt);
                return this;
            }

            /// @brief Replaces the help formatter with a callable.
            ///
            /// @param fmt Renders the whole help page.
            /// @return A pointer to this application, for chaining.
            auto formatter_fn(std::function<std::string(const app_t *, std::string, app_format_mode_t)>
                                  fmt) -> app_t *
            {
                formatter_ = std::make_shared<formatter_lambda_t>(std::move(fmt));
                return this;
            }

            /// @brief Replaces the configuration reader and writer.
            ///
            /// @param fmt The converter to use.
            /// @return A pointer to this application, for chaining.
            auto config_formatter(std::shared_ptr<config_t> fmt) -> app_t *
            {
                config_formatter_ = std::move(fmt);
                return this;
            }

            /// @brief Reports whether this subcommand appeared on the command line.
            ///
            /// @return `true` if it was parsed at least once.
            [[nodiscard]] auto parsed() const -> bool
            {
                return parsed_ > 0;
            }

            /// @brief Returns the settings newly added options inherit.
            ///
            /// @return A pointer to the option defaults.
            auto option_defaults() -> option_defaults_t *
            {
                return &option_defaults_;
            }

            ///@}
            /// @name Adding options
            ///@{

            /// @brief Adds an option driven by a callback.
            ///
            /// The other `add_option` overloads build on this one.
            ///
            /// @param option_name The name specification, for example `"-f,--file"`.
            /// @param option_callback Consumes the collected results.
            /// @param option_description The description shown in help output.
            /// @param defaulted Whether the bound value already holds a usable default.
            /// @param func Produces the printed default.
            /// @return A pointer to the new option, for chaining.
            auto add_option(std::string option_name, callback_t option_callback, std::string option_description = "",
                            bool defaulted = false,
                            std::function<std::string()> func = {}) -> option_t *;

            /// @brief Adds an option bound to a variable.
            ///
            /// The variable's type determines how many values the option takes and how
            /// they are converted. Supply @p convert_to_t when the value should be read
            /// as one type and stored as another.
            ///
            /// @code
            /// std::string filename;
            /// program.add_option("filename", filename, "description of filename");
            /// @endcode
            ///
            /// @tparam assign_to_t The type of the bound variable.
            /// @tparam convert_to_t The type values are parsed as.
            /// @param option_name The name specification.
            /// @param variable The variable to fill.
            /// @param option_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename assign_to_t, typename convert_to_t = assign_to_t>
                requires(!std::is_const_v<convert_to_t>)
            auto add_option(std::string option_name, assign_to_t &variable,
                            std::string option_description = "") -> option_t *
            {
                auto fun = [&variable](const results_t &res) {
                    return detail::lexical_conversion<assign_to_t, convert_to_t>(res, variable);
                };

                option_t *opt = add_option(option_name, fun, option_description, false, [&variable] {
                    return detail::checked_to_string<assign_to_t, convert_to_t>(variable);
                });
                opt->type_name(detail::type_name<convert_to_t>());

                // Bound to named values because some standard libraries define std::max
                // in terms of references, and a reference to a temporary would dangle.
                const auto t_count = detail::type_count_v<assign_to_t>;
                const auto xc_count = detail::type_count_v<convert_to_t>;
                opt->type_size(detail::type_count_min_v<convert_to_t>, (std::max)(t_count, xc_count));
                opt->expected(detail::expected_count_v<convert_to_t>);
                opt->run_callback_for_default();
                return opt;
            }

            /// @brief Adds an option bound to a variable, without a printed default.
            ///
            /// For types that have no usable string representation.
            ///
            /// @tparam assign_to_t The type of the bound variable.
            /// @param option_name The name specification.
            /// @param variable The variable to fill.
            /// @param option_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename assign_to_t>
                requires(!std::is_const_v<assign_to_t>)
            auto add_option_no_stream(std::string option_name, assign_to_t &variable,
                                      std::string option_description = "") -> option_t *
            {
                auto fun = [&variable](const results_t &res) {
                    return detail::lexical_conversion<assign_to_t, assign_to_t>(res, variable);
                };

                option_t *opt =
                    add_option(option_name, fun, option_description, false, [] { return std::string {}; });
                opt->type_name(detail::type_name<assign_to_t>());
                opt->type_size(detail::type_count_min_v<assign_to_t>, detail::type_count_v<assign_to_t>);
                opt->expected(detail::expected_count_v<assign_to_t>);
                opt->run_callback_for_default();
                return opt;
            }

            /// @brief Adds an option that hands its converted value to a callback.
            ///
            /// @tparam arg_type_t The type values are converted to.
            /// @param option_name The name specification.
            /// @param func Receives the converted value.
            /// @param option_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename arg_type_t>
            auto add_option_function(std::string option_name,
                                     std::function<void(const arg_type_t &)> func,
                                     std::string option_description = "") -> option_t *
            {
                auto fun = [f = std::move(func)](const results_t &res) {
                    arg_type_t variable;
                    const bool result = detail::lexical_conversion<arg_type_t, arg_type_t>(res, variable);
                    if (result)
                    {
                        f(variable);
                    }
                    return result;
                };

                option_t *opt = add_option(option_name, std::move(fun), option_description, false);
                opt->type_name(detail::type_name<arg_type_t>());
                opt->type_size(detail::type_count_min_v<arg_type_t>, detail::type_count_v<arg_type_t>);
                opt->expected(detail::expected_count_v<arg_type_t>);
                return opt;
            }

            /// @brief Adds an option with no description and no bound variable.
            ///
            /// @param option_name The name specification.
            /// @return A pointer to the new option, for chaining.
            auto add_option(std::string option_name) -> option_t *
            {
                return add_option(option_name, callback_t {}, std::string {}, false);
            }

            /// @brief Adds an option with a description but no bound variable.
            ///
            /// @tparam T A const string-like type.
            /// @param option_name The name specification.
            /// @param option_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename T>
                requires(std::is_const_v<T> && std::is_constructible_v<std::string, T>)
            auto add_option(std::string option_name, T &option_description) -> option_t *
            {
                return add_option(option_name, callback_t(), option_description, false);
            }

            /// @brief Replaces the help flag.
            ///
            /// An empty @p flag_name removes the help flag entirely.
            ///
            /// @param flag_name The name specification.
            /// @param help_description The description shown in help output.
            /// @return A pointer to the new option, or `nullptr` if it was removed.
            auto set_help_flag(std::string flag_name = "", const std::string &help_description = "") -> option_t *;

            /// @brief Replaces the expanded-help flag.
            ///
            /// An empty @p help_name removes it entirely.
            ///
            /// @param help_name The name specification.
            /// @param help_description The description shown in help output.
            /// @return A pointer to the new option, or `nullptr` if it was removed.
            auto set_help_all_flag(std::string help_name = "",
                                   const std::string &help_description = "") -> option_t *;

            /// @brief Replaces the version flag with a fixed version string.
            ///
            /// @param flag_name The name specification.
            /// @param version_string The version text to print.
            /// @param version_help The description shown in help output.
            /// @return A pointer to the new option, or `nullptr` if it was removed.
            auto set_version_flag(std::string flag_name = "", const std::string &version_string = "",
                                  const std::string &version_help =
                                      "Display program version information and exit") -> option_t *;

            /// @brief Replaces the version flag with one that generates its text.
            ///
            /// @param flag_name The name specification.
            /// @param vfunc Produces the version text.
            /// @param version_help The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            auto set_version_flag(std::string flag_name, std::function<std::string()> vfunc,
                                  const std::string &version_help =
                                      "Display program version information and exit") -> option_t *;

        private:
            /// @brief Adds a flag, shared by every public `add_flag` overload.
            ///
            /// @param flag_name The name specification.
            /// @param fun Consumes the collected results.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            auto _add_flag_internal(std::string flag_name, callback_t fun,
                                    std::string flag_description) -> option_t *;

        public:
            /// @brief Adds a flag with no description and no bound variable.
            ///
            /// @param flag_name The name specification.
            /// @return A pointer to the new option, for chaining.
            auto add_flag(std::string flag_name) -> option_t *
            {
                return _add_flag_internal(flag_name, callback_t(), std::string {});
            }

            /// @brief Adds a flag with a description but no bound variable.
            ///
            /// Takes a const string or an rvalue string. A non-const lvalue string binds
            /// to the overload below instead, and receives the flag's result.
            ///
            /// @tparam T A const or rvalue string-like type.
            /// @param flag_name The name specification.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename T>
                requires((std::is_const_v<std::remove_reference_t<T>> || std::is_rvalue_reference_v<T &&>) &&
                         std::is_constructible_v<std::string, std::remove_reference_t<T>>)
            auto add_flag(std::string flag_name, T &&flag_description) -> option_t *
            {
                return _add_flag_internal(flag_name, callback_t(), std::forward<T>(flag_description));
            }

            /// @brief Adds a flag bound to a variable.
            ///
            /// Accepts anything that is not a container and not callable: `bool`, an
            /// enumeration, a string, an integer counter, or any type constructible from
            /// a string. An integer wider than a byte is treated as a counter.
            ///
            /// @tparam T The type of the bound variable.
            /// @param flag_name The name specification.
            /// @param flag_result The variable to fill.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename T>
                requires(!detail::mutable_container<T> && !std::is_const_v<T> &&
                         !std::is_constructible_v<std::function<void(std::int64_t)>, T>)
            auto add_flag(std::string flag_name, T &flag_result,
                          std::string flag_description = "") -> option_t *
            {
                callback_t fun = [&flag_result](const results_t &res) {
                    using detail::lexical_cast;
                    return lexical_cast(res[0], flag_result);
                };
                auto *opt = _add_flag_internal(flag_name, std::move(fun), std::move(flag_description));
                return detail::default_flag_modifiers<T>(opt);
            }

            /// @brief Adds a flag that collects a value for every appearance.
            ///
            /// @tparam T The element type of the bound vector.
            /// @param flag_name The name specification.
            /// @param flag_results The vector to append to.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            template <typename T>
                requires(!std::is_assignable_v<std::function<void(std::int64_t)> &, T>)
            auto add_flag(std::string flag_name, std::vector<T> &flag_results,
                          std::string flag_description = "") -> option_t *
            {
                callback_t fun = [&flag_results](const results_t &res) {
                    bool retval = true;
                    for (const auto &elem : res)
                    {
                        using detail::lexical_cast;
                        flag_results.emplace_back();
                        retval &= lexical_cast(elem, flag_results.back());
                    }
                    return retval;
                };
                return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description))
                    ->multi_option_policy(multi_option_policy_t::take_all)
                    ->run_callback_for_default();
            }

            /// @brief Adds a flag that calls a function taking no arguments.
            ///
            /// @param flag_name The name specification.
            /// @param function Called once when the flag is used.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            auto add_flag_callback(std::string flag_name, std::function<void()> function,
                                   std::string flag_description = "") -> option_t *;

            /// @brief Adds a flag that calls a function with its accumulated count.
            ///
            /// @param flag_name The name specification.
            /// @param function Called with the flag's value.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            auto add_flag_function(std::string flag_name, std::function<void(std::int64_t)> function,
                                   std::string flag_description = "") -> option_t *;

            /// @brief Adds a flag that calls a function with its accumulated count.
            ///
            /// Aliases @ref add_flag_function.
            ///
            /// @param flag_name The name specification.
            /// @param function Called with the flag's value.
            /// @param flag_description The description shown in help output.
            /// @return A pointer to the new option, for chaining.
            auto add_flag(std::string flag_name, std::function<void(std::int64_t)> function,
                          std::string flag_description = "") -> option_t *
            {
                return add_flag_function(std::move(flag_name), std::move(function), std::move(flag_description));
            }

            /// @brief Sets the option that names a configuration file.
            ///
            /// An empty @p option_name removes it.
            ///
            /// @param option_name The name specification.
            /// @param default_filename The file read when the option is not given.
            /// @param help_message The description shown in help output.
            /// @param config_required Whether the file must exist.
            /// @return A pointer to the new option, or `nullptr` if it was removed.
            auto set_config(std::string option_name = "", std::string default_filename = "",
                            const std::string &help_message = "Read an ini file",
                            bool config_required = false) -> option_t *;

            /// @brief Removes an option from this application.
            ///
            /// @param opt The option to remove.
            /// @return `true` if the option was found and removed.
            auto remove_option(option_t *opt) -> bool;

            /// @brief Adds an option group.
            ///
            /// An option group is a subcommand with no name: it groups options in help
            /// output and can carry its own requirements, but is never named on the
            /// command line.
            ///
            /// @tparam T The group type to create.
            /// @param group_name The group name.
            /// @param group_description The description shown in help output.
            /// @return A pointer to the new group, for chaining.
            /// @throws cli::incorrect_construction_t If @p group_name contains a newline
            /// or null character.
            template <typename T = option_group_t>
            auto add_option_group(std::string group_name, std::string group_description = "") -> T *
            {
                if (!detail::valid_alias_name_string(group_name))
                {
                    throw incorrect_construction_t("option group names may not contain newlines or null characters");
                }
                auto option_group = std::make_shared<T>(std::move(group_description), group_name, this);
                option_group->fallthrough(false);
                auto *ptr = option_group.get();

                app_ptr_t app_ptr = std::static_pointer_cast<app_t>(option_group);
                // An option group neither inherits the parent's footer nor carries a
                // help flag of its own.
                app_ptr->footer_ = "";
                app_ptr->set_help_flag();
                add_subcommand(std::move(app_ptr));
                return ptr;
            }

            ///@}
            /// @name Subcommands
            ///@{

            /// @brief Adds a subcommand.
            ///
            /// The subcommand inherits the inheritable settings and the option defaults.
            ///
            /// @param subcommand_name The subcommand name.
            /// @param subcommand_description The description shown in help output.
            /// @return A pointer to the new subcommand, for chaining.
            auto add_subcommand(std::string subcommand_name = "",
                                std::string subcommand_description = "") -> app_t *;

            /// @brief Adds an already-constructed application as a subcommand.
            ///
            /// @param subcom The subcommand to adopt.
            /// @return A pointer to the subcommand, for chaining.
            auto add_subcommand(app_ptr_t subcom) -> app_t *;

            /// @brief Removes a subcommand from this application.
            ///
            /// @param subcom The subcommand to remove.
            /// @return `true` if the subcommand was found and removed.
            auto remove_subcommand(app_t *subcom) -> bool;

            /// @brief Finds a subcommand by pointer.
            ///
            /// @param subcom The subcommand to look for; `nullptr` returns the first one.
            /// @return A pointer to the subcommand.
            /// @throws cli::option_not_found_t If it is not a subcommand of this application.
            auto get_subcommand(const app_t *subcom) const -> app_t *;

            /// @brief Finds a subcommand by name.
            ///
            /// @param subcom The name to look for.
            /// @return A pointer to the subcommand.
            /// @throws cli::option_not_found_t If no subcommand matches.
            [[nodiscard]] auto get_subcommand(std::string subcom) const -> app_t *;

            /// @brief Finds a subcommand by name, without throwing.
            ///
            /// @param subcom The name to look for.
            /// @return A pointer to the subcommand, or `nullptr` if none matches.
            [[nodiscard]] auto get_subcommand_no_throw(std::string subcom) const noexcept -> app_t *;

            /// @brief Finds a subcommand by position.
            ///
            /// @param index The position to look up.
            /// @return A pointer to the subcommand.
            /// @throws cli::option_not_found_t If @p index is out of range.
            [[nodiscard]] auto get_subcommand(int index = 0) const -> app_t *;

            /// @brief Finds a subcommand by pointer and returns an owning handle.
            ///
            /// @param subcom The subcommand to look for.
            /// @return A shared handle to the subcommand.
            /// @throws cli::option_not_found_t If it is not a subcommand of this application.
            auto get_subcommand_ptr(app_t *subcom) const -> app_ptr_t;

            /// @brief Finds a subcommand by name and returns an owning handle.
            ///
            /// @param subcom The name to look for.
            /// @return A shared handle to the subcommand.
            /// @throws cli::option_not_found_t If no subcommand matches.
            [[nodiscard]] auto get_subcommand_ptr(std::string subcom) const -> app_ptr_t;

            /// @brief Finds a subcommand by position and returns an owning handle.
            ///
            /// @param index The position to look up.
            /// @return A shared handle to the subcommand.
            /// @throws cli::option_not_found_t If @p index is out of range.
            [[nodiscard]] auto get_subcommand_ptr(int index = 0) const -> app_ptr_t;

            /// @brief Finds an option group by name.
            ///
            /// @param group_name The group name.
            /// @return A pointer to the group.
            /// @throws cli::option_not_found_t If no group matches.
            [[nodiscard]] auto get_option_group(std::string group_name) const -> app_t *;

            /// @brief Returns how many times this subcommand was used.
            ///
            /// The main application reports 1, as does an unnamed subcommand unless a
            /// callback changes it.
            ///
            /// @return The use count.
            [[nodiscard]] auto count() const -> std::size_t
            {
                return parsed_;
            }

            /// @brief Returns how many arguments were consumed by options and subcommands.
            ///
            /// Arguments treated as extras are not counted.
            ///
            /// @return The argument count.
            [[nodiscard]] auto count_all() const -> std::size_t;

            /// @brief Sets the help group this subcommand is listed under.
            ///
            /// @param group_name The group name.
            /// @return A pointer to this application, for chaining.
            auto group(std::string group_name) -> app_t *
            {
                group_ = std::move(group_name);
                return this;
            }

            /// @brief Requires at least one subcommand to be used.
            ///
            /// @return A pointer to this application, for chaining.
            auto require_subcommand() -> app_t *
            {
                require_subcommand_min_ = 1;
                require_subcommand_max_ = 0;
                return this;
            }

            /// @brief Requires a given number of subcommands.
            ///
            /// A negative value sets a maximum instead of an exact count; `0` means
            /// unlimited. Does not affect a help request. The maximum is inheritable.
            ///
            /// @param value The count, or its negation for a maximum.
            /// @return A pointer to this application, for chaining.
            auto require_subcommand(int value) -> app_t *
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

            /// @brief Requires a number of subcommands within a range.
            ///
            /// A maximum of `0` means unlimited. The maximum is inheritable.
            ///
            /// @param min The smallest acceptable count.
            /// @param max The largest acceptable count.
            /// @return A pointer to this application, for chaining.
            auto require_subcommand(std::size_t min, std::size_t max) -> app_t *
            {
                require_subcommand_min_ = min;
                require_subcommand_max_ = max;
                return this;
            }

            /// @brief Requires at least one option to be used.
            ///
            /// @return A pointer to this application, for chaining.
            auto require_option() -> app_t *
            {
                require_option_min_ = 1;
                require_option_max_ = 0;
                return this;
            }

            /// @brief Requires a given number of options.
            ///
            /// A negative value sets a maximum instead of an exact count; `0` means
            /// unlimited. Does not affect a help request.
            ///
            /// @param value The count, or its negation for a maximum.
            /// @return A pointer to this application, for chaining.
            auto require_option(int value) -> app_t *
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

            /// @brief Requires a number of options within a range.
            ///
            /// A maximum of `0` means unlimited.
            ///
            /// @param min The smallest acceptable count.
            /// @param max The largest acceptable count.
            /// @return A pointer to this application, for chaining.
            auto require_option(std::size_t min, std::size_t max) -> app_t *
            {
                require_option_min_ = min;
                require_option_max_ = max;
                return this;
            }

            /// @brief Lets unrecognised options fall through to the parent command.
            ///
            /// Inherited from the parent, and usually set there.
            ///
            /// @param value Whether to allow fallthrough.
            /// @return A pointer to this application, for chaining.
            auto fallthrough(bool value = true) -> app_t *
            {
                fallthrough_ = value;
                return this;
            }

            /// @brief Lets a parent's subcommands be recognised from within this one.
            ///
            /// @param value Whether to allow subcommand fallthrough.
            /// @return A pointer to this application, for chaining.
            auto subcommand_fallthrough(bool value = true) -> app_t *
            {
                subcommand_fallthrough_ = value;
                return this;
            }

            /// @brief Reports whether this subcommand appeared on the command line.
            ///
            /// Lets a subcommand be tested directly: `if (*sub) { ... }`.
            ///
            /// @return `true` if it was parsed at least once.
            [[nodiscard]] explicit operator bool() const
            {
                return parsed_ > 0;
            }

            ///@}
            /// @name Extras for subclassing
            ///@{

            /// @brief Hook run after parsing but before the callbacks.
            ///
            /// Does not run if an error or a help request was thrown.
            virtual auto pre_callback() -> void
            {
            }

            ///@}
            /// @name Parsing
            ///@{

            /// @brief Discards everything collected by a previous parse.
            auto clear() -> void;

            /// @brief Parses the command line.
            ///
            /// Call once every option and subcommand has been added.
            ///
            /// @param argc The argument count, as given to `main`.
            /// @param argv The argument array, as given to `main`.
            /// @throws cli::parse_error_t If the command line cannot be parsed.
            auto parse(int argc, const char *const *argv) -> void;

            /// @brief Parses a wide command line.
            ///
            /// The arguments are narrowed to UTF-8 first.
            ///
            /// @param argc The argument count.
            /// @param argv The argument array.
            /// @throws cli::parse_error_t If the command line cannot be parsed.
            auto parse(int argc, const wchar_t *const *argv) -> void;

        private:
            /// @brief Parses an argument array of either character type.
            ///
            /// @tparam char_t The character type of the argument array.
            /// @param argc The argument count.
            /// @param argv The argument array.
            template <class char_t> auto parse_char_t(int argc, const char_t *const *argv) -> void;

        public:
            /// @brief Parses a whole command line held in one string.
            ///
            /// The string is split into arguments and handed to the vector overload.
            ///
            /// @param commandline The command line to parse.
            /// @param program_name_included Whether @p commandline starts with the
            /// program name.
            /// @throws cli::parse_error_t If the command line cannot be parsed.
            auto parse(std::string commandline, bool program_name_included = false) -> void;

            /// @brief Parses a whole wide command line held in one string.
            ///
            /// @param commandline The command line to parse.
            /// @param program_name_included Whether @p commandline starts with the
            /// program name.
            /// @throws cli::parse_error_t If the command line cannot be parsed.
            auto parse(std::wstring commandline, bool program_name_included = false) -> void;

            /// @brief Parses a list of arguments, in reverse order.
            ///
            /// This is where the work happens; the other overloads funnel into it.
            /// @p args is left holding whatever was not consumed.
            ///
            /// @param[in,out] args The arguments, last one first.
            /// @throws cli::parse_error_t If the arguments cannot be parsed.
            auto parse(std::vector<std::string> &args) -> void;

            /// @brief Parses a list of arguments, in reverse order.
            ///
            /// @param args The arguments, last one first.
            /// @throws cli::parse_error_t If the arguments cannot be parsed.
            auto parse(std::vector<std::string> &&args) -> void;

            /// @brief Parses arguments read from a stream as a configuration file.
            ///
            /// @param input The stream to read.
            /// @throws cli::parse_error_t If the contents cannot be parsed.
            auto parse_from_stream(std::istream &input) -> void;

            /// @brief Sets how an error is rendered by @ref exit.
            ///
            /// @param function Receives the application and the error, returns the text
            /// to print.
            auto failure_message(std::function<std::string(const app_t *, const error_t &)> function)
                -> void
            {
                failure_message_ = std::move(function);
            }

            /// @brief Prints an error and returns the exit code to give the process.
            ///
            /// A help or version request is printed to @p out; anything else goes to
            /// @p err.
            ///
            /// @param e The error to report.
            /// @param out The stream used for successful early exits.
            /// @param err The stream used for failures.
            /// @return The process exit code.
            auto exit(const error_t &e, std::ostream &out = std::cout, std::ostream &err = std::cerr) const -> int;

            ///@}
            /// @name Post parsing
            ///@{

            /// @brief Returns how many times an option was used.
            ///
            /// @param option_name The option to look up.
            /// @return The use count.
            /// @throws cli::option_not_found_t If no option matches.
            [[nodiscard]] auto count(std::string option_name) const -> std::size_t
            {
                return get_option(option_name)->count();
            }

            /// @brief Returns the subcommands that were used, in command-line order.
            ///
            /// @return The parsed subcommands.
            [[nodiscard]] auto get_subcommands() const -> const std::vector<app_t *> &
            {
                return parsed_subcommands_;
            }

            /// @brief Returns the subcommands matching a filter, as defined.
            ///
            /// @param filter Selects which subcommands to return; empty returns all.
            /// @return The matching subcommands.
            auto get_subcommands(const std::function<bool(const app_t *)> &filter) const
                -> std::vector<const app_t *>;

            /// @brief Returns the subcommands matching a filter, as defined.
            ///
            /// @param filter Selects which subcommands to return; empty returns all.
            /// @return The matching subcommands.
            auto get_subcommands(const std::function<bool(app_t *)> &filter) -> std::vector<app_t *>;

            /// @brief Reports whether a subcommand was used.
            ///
            /// @param subcom The subcommand to check.
            /// @return `true` if it was used.
            /// @throws cli::option_not_found_t If it is not a subcommand of this application.
            auto got_subcommand(const app_t *subcom) const -> bool
            {
                // Routed through get_subcommand so that an unrelated pointer is reported
                // rather than silently answering false.
                return get_subcommand(subcom)->parsed_ > 0;
            }

            /// @brief Reports whether a named subcommand was used.
            ///
            /// @param subcommand_name The subcommand to check.
            /// @return `true` if it exists and was used.
            [[nodiscard]] auto got_subcommand(std::string subcommand_name) const noexcept -> bool
            {
                const app_t *sub = get_subcommand_no_throw(std::move(subcommand_name));
                return (sub != nullptr) ? (sub->parsed_ > 0) : false;
            }

            /// @brief Forbids an option from being used alongside this subcommand.
            ///
            /// @param opt The option to exclude.
            /// @return A pointer to this application, for chaining.
            /// @throws cli::option_not_found_t If @p opt is null.
            auto excludes(option_t *opt) -> app_t *
            {
                if (opt == nullptr)
                {
                    throw option_not_found_t("nullptr passed");
                }
                exclude_options_.insert(opt);
                return this;
            }

            /// @brief Forbids a subcommand from being used alongside this one.
            ///
            /// The exclusion is recorded on both subcommands.
            ///
            /// @param app The subcommand to exclude.
            /// @return A pointer to this application, for chaining.
            /// @throws cli::option_not_found_t If @p app is null or is this application.
            auto excludes(app_t *app) -> app_t *
            {
                if (app == nullptr)
                {
                    throw option_not_found_t("nullptr passed");
                }
                if (app == this)
                {
                    throw option_not_found_t("cannot self reference in needs");
                }
                auto res = exclude_subcommands_.insert(app);
                if (res.second)
                {
                    app->exclude_subcommands_.insert(this);
                }
                return this;
            }

            /// @brief Requires an option to be used alongside this subcommand.
            ///
            /// Not mutual: the option does not come to require this subcommand.
            ///
            /// @param opt The required option.
            /// @return A pointer to this application, for chaining.
            /// @throws cli::option_not_found_t If @p opt is null.
            auto needs(option_t *opt) -> app_t *
            {
                if (opt == nullptr)
                {
                    throw option_not_found_t("nullptr passed");
                }
                need_options_.insert(opt);
                return this;
            }

            /// @brief Requires a subcommand to be used alongside this one.
            ///
            /// Not mutual.
            ///
            /// @param app The required subcommand.
            /// @return A pointer to this application, for chaining.
            /// @throws cli::option_not_found_t If @p app is null or is this application.
            auto needs(app_t *app) -> app_t *
            {
                if (app == nullptr)
                {
                    throw option_not_found_t("nullptr passed");
                }
                if (app == this)
                {
                    throw option_not_found_t("cannot self reference in needs");
                }
                need_subcommands_.insert(app);
                return this;
            }

            /// @brief Drops an option exclusion.
            ///
            /// @param opt The option to stop excluding.
            /// @return `true` if the exclusion was present.
            auto remove_excludes(option_t *opt) -> bool;

            /// @brief Drops a subcommand exclusion.
            ///
            /// @param app The subcommand to stop excluding.
            /// @return `true` if the exclusion was present.
            auto remove_excludes(app_t *app) -> bool;

            /// @brief Drops an option requirement.
            ///
            /// @param opt The option to stop requiring.
            /// @return `true` if the requirement was present.
            auto remove_needs(option_t *opt) -> bool;

            /// @brief Drops a subcommand requirement.
            ///
            /// @param app The subcommand to stop requiring.
            /// @return `true` if the requirement was present.
            auto remove_needs(app_t *app) -> bool;

            ///@}
            /// @name Help
            ///@{

            /// @brief Sets the usage line shown after the description.
            ///
            /// @param usage_string The usage line.
            /// @return A pointer to this application, for chaining.
            auto usage(std::string usage_string) -> app_t *
            {
                usage_ = std::move(usage_string);
                return this;
            }

            /// @brief Sets a callable producing the usage line.
            ///
            /// @param usage_function Produces the usage line.
            /// @return A pointer to this application, for chaining.
            auto usage(std::function<std::string()> usage_function) -> app_t *
            {
                usage_callback_ = std::move(usage_function);
                return this;
            }

            /// @brief Sets the footer shown after all options.
            ///
            /// @param footer_string The footer text.
            /// @return A pointer to this application, for chaining.
            auto footer(std::string footer_string) -> app_t *
            {
                footer_ = std::move(footer_string);
                return this;
            }

            /// @brief Sets a callable producing the footer.
            ///
            /// @param footer_function Produces the footer text.
            /// @return A pointer to this application, for chaining.
            auto footer(std::function<std::string()> footer_function) -> app_t *
            {
                footer_callback_ = std::move(footer_function);
                return this;
            }

            /// @brief Writes the current values out as configuration text.
            ///
            /// @return The configuration text, covering only options that were set.
            [[nodiscard]] auto config_to_str() const -> std::string
            {
                return config_to_str(config_output_mode_t::active, false);
            }

            /// @brief Writes the current values out as configuration text.
            ///
            /// @param mode How much of the application to write.
            /// @param write_description Include descriptions as comments.
            /// @return The configuration text.
            [[nodiscard]] auto config_to_str(config_output_mode_t mode,
                                             bool write_description = false) const -> std::string
            {
                return config_formatter_->to_config(this, mode, write_description, "");
            }

            /// @brief Writes the current values out as configuration text.
            ///
            /// @deprecated Use the @ref config_output_mode_t overload instead.
            ///
            /// @param default_also Include options left at their defaults.
            /// @param write_description Include descriptions as comments.
            /// @return The configuration text.
            [[nodiscard]] auto config_to_str(bool default_also,
                                             bool write_description = false) const -> std::string
            {
                return config_to_str(
                    default_also ? config_output_mode_t::all_defaults : config_output_mode_t::active,
                    write_description);
            }

            /// @brief Renders the help page using the configured formatter.
            ///
            /// Covers one subcommand at a time.
            ///
            /// @param prev The name to present this application under.
            /// @param mode How much detail to include.
            /// @return The rendered help page.
            [[nodiscard]] auto help(std::string prev = "",
                                    app_format_mode_t mode = app_format_mode_t::normal) const -> std::string;

            /// @brief Renders the version string.
            ///
            /// @return The version text.
            [[nodiscard]] auto version() const -> std::string;

            ///@}
            /// @name Getters
            ///@{

            /// @brief Returns the help formatter.
            ///
            /// @return A shared handle to the formatter.
            [[nodiscard]] auto get_formatter() const -> std::shared_ptr<formatter_base_t>
            {
                return formatter_;
            }

            /// @brief Returns the configuration reader and writer.
            ///
            /// @return A shared handle to the converter.
            [[nodiscard]] auto get_config_formatter() const -> std::shared_ptr<config_t>
            {
                return config_formatter_;
            }

            /// @brief Returns the configuration converter as a @ref config_base_t.
            ///
            /// @return A shared handle to the converter, or empty if it is not a
            /// @ref config_base_t.
            [[nodiscard]] auto get_config_formatter_base() const -> std::shared_ptr<config_base_t>
            {
                return std::dynamic_pointer_cast<config_base_t>(config_formatter_);
            }

            /// @brief Returns the description shown in help output.
            ///
            /// @return The description.
            [[nodiscard]] auto get_description() const -> const std::string &
            {
                return description_;
            }

            /// @brief Sets the description shown in help output.
            ///
            /// @param app_description The description.
            /// @return A pointer to this application, for chaining.
            auto description(std::string app_description) -> app_t *
            {
                description_ = std::move(app_description);
                return this;
            }

            /// @brief Returns the options matching a filter.
            ///
            /// @param filter Selects which options to return; empty returns all.
            /// @return The matching options.
            [[nodiscard]] auto get_options(const std::function<bool(const option_t *)> &filter = {})
                const -> std::vector<const option_t *>;

            /// @brief Returns the options matching a filter.
            ///
            /// @param filter Selects which options to return; empty returns all.
            /// @return The matching options.
            [[nodiscard]] auto get_options(const std::function<bool(option_t *)> &filter = {})
                -> std::vector<option_t *>;

            /// @brief Finds an option by name, without throwing.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option, or `nullptr` if none matches.
            [[nodiscard]] auto get_option_no_throw(std::string option_name) noexcept -> option_t *;

            /// @brief Finds an option by name, without throwing.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option, or `nullptr` if none matches.
            [[nodiscard]] auto get_option_no_throw(std::string option_name) const noexcept -> const option_t *;

            /// @brief Finds an option by name.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option.
            /// @throws cli::option_not_found_t If no option matches.
            [[nodiscard]] auto get_option(std::string option_name) const -> const option_t *;

            /// @brief Finds an option by name.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option.
            /// @throws cli::option_not_found_t If no option matches.
            [[nodiscard]] auto get_option(std::string option_name) -> option_t *;

            /// @brief Finds an option by name.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option.
            /// @throws cli::option_not_found_t If no option matches.
            [[nodiscard]] auto operator[](const std::string &option_name) const -> const option_t *
            {
                return get_option(option_name);
            }

            /// @brief Finds an option by name.
            ///
            /// @param option_name The name to look for.
            /// @return A pointer to the option.
            /// @throws cli::option_not_found_t If no option matches.
            [[nodiscard]] auto operator[](const char *option_name) const -> const option_t *
            {
                return get_option(option_name);
            }

            /// @brief Reports whether name matching ignores case.
            ///
            /// @return `true` if case is ignored.
            [[nodiscard]] auto get_ignore_case() const -> bool
            {
                return ignore_case_;
            }

            /// @brief Reports whether name matching ignores underscores.
            ///
            /// @return `true` if underscores are ignored.
            [[nodiscard]] auto get_ignore_underscore() const -> bool
            {
                return ignore_underscore_;
            }

            /// @brief Reports whether options fall through to the parent command.
            ///
            /// @return `true` if fallthrough is enabled.
            [[nodiscard]] auto get_fallthrough() const -> bool
            {
                return fallthrough_;
            }

            /// @brief Reports whether a parent's subcommands are recognised here.
            ///
            /// @return `true` if subcommand fallthrough is enabled.
            [[nodiscard]] auto get_subcommand_fallthrough() const -> bool
            {
                return subcommand_fallthrough_;
            }

            /// @brief Reports whether Windows-style options are accepted.
            ///
            /// @return `true` if `/opt` is accepted.
            [[nodiscard]] auto get_allow_windows_style_options() const -> bool
            {
                return allow_windows_style_options_;
            }

            /// @brief Reports whether positionals must appear after every option.
            ///
            /// @return `true` if positionals come last.
            [[nodiscard]] auto get_positionals_at_end() const -> bool
            {
                return positionals_at_end_;
            }

            /// @brief Reports whether this subcommand can be triggered from a configuration file.
            ///
            /// @return `true` if it is configurable.
            [[nodiscard]] auto get_configurable() const -> bool
            {
                return configurable_;
            }

            /// @brief Returns the help group this subcommand is listed under.
            ///
            /// @return The group name.
            [[nodiscard]] auto get_group() const -> const std::string &
            {
                return group_;
            }

            /// @brief Returns the usage line.
            ///
            /// When a usage callback is set, its output precedes any fixed usage text.
            ///
            /// @return The usage line.
            [[nodiscard]] auto get_usage() const -> std::string
            {
                return (usage_callback_) ? usage_callback_() + '\n' + usage_ : usage_;
            }

            /// @brief Returns the footer.
            ///
            /// When a footer callback is set, its output precedes any fixed footer text.
            ///
            /// @return The footer.
            [[nodiscard]] auto get_footer() const -> std::string
            {
                return (footer_callback_) ? footer_callback_() + '\n' + footer_ : footer_;
            }

            /// @brief Returns the smallest number of subcommands that must be used.
            ///
            /// @return The count.
            [[nodiscard]] auto get_require_subcommand_min() const -> std::size_t
            {
                return require_subcommand_min_;
            }

            /// @brief Returns the largest number of subcommands accepted.
            ///
            /// @return The count; `0` means unlimited.
            [[nodiscard]] auto get_require_subcommand_max() const -> std::size_t
            {
                return require_subcommand_max_;
            }

            /// @brief Returns the smallest number of options that must be used.
            ///
            /// @return The count.
            [[nodiscard]] auto get_require_option_min() const -> std::size_t
            {
                return require_option_min_;
            }

            /// @brief Returns the largest number of options accepted.
            ///
            /// @return The count; `0` means unlimited.
            [[nodiscard]] auto get_require_option_max() const -> std::size_t
            {
                return require_option_max_;
            }

            /// @brief Reports whether parsing stops at an unrecognised argument.
            ///
            /// @return `true` if prefix-command mode is enabled in any form.
            [[nodiscard]] auto get_prefix_command() const -> bool
            {
                return prefix_command_ != prefix_command_mode_t::off;
            }

            /// @brief Returns when parsing stops early.
            ///
            /// @return The prefix-command mode.
            [[nodiscard]] auto get_prefix_command_mode() const -> prefix_command_mode_t
            {
                return prefix_command_;
            }

            /// @brief Reports whether unmatched arguments are accepted.
            ///
            /// @return `true` if they are captured rather than reported.
            [[nodiscard]] auto get_allow_extras() const -> bool
            {
                return allow_extras_ > extras_mode_t::ignore;
            }

            /// @brief Returns what happens to unmatched arguments.
            ///
            /// @return The extras mode.
            [[nodiscard]] auto get_allow_extras_mode() const -> extras_mode_t
            {
                return allow_extras_;
            }

            /// @brief Reports whether this subcommand must be used.
            ///
            /// @return `true` if it is required.
            [[nodiscard]] auto get_required() const -> bool
            {
                return required_;
            }

            /// @brief Reports whether this subcommand is disabled.
            ///
            /// @return `true` if it is disabled.
            [[nodiscard]] auto get_disabled() const -> bool
            {
                return disabled_;
            }

            /// @brief Reports whether this subcommand is hidden from the processed list.
            ///
            /// @return `true` if it is hidden.
            [[nodiscard]] auto get_silent() const -> bool
            {
                return silent_;
            }

            /// @brief Reports whether non-standard option names are accepted.
            ///
            /// @return `true` if they are accepted.
            [[nodiscard]] auto get_allow_non_standard_option_names() const -> bool
            {
                return allow_non_standard_options_;
            }

            /// @brief Reports whether subcommands may be matched by a name prefix.
            ///
            /// @return `true` if prefix matching is enabled.
            [[nodiscard]] auto get_allow_subcommand_prefix_matching() const -> bool
            {
                return allow_prefix_matching_;
            }

            /// @brief Reports whether the callback runs as soon as parsing finishes.
            ///
            /// @return `true` if the callback is immediate.
            [[nodiscard]] auto get_immediate_callback() const -> bool
            {
                return immediate_callback_;
            }

            /// @brief Reports whether @ref clear leaves this subcommand disabled.
            ///
            /// @return `true` if it is disabled by default.
            [[nodiscard]] auto get_disabled_by_default() const -> bool
            {
                return (default_startup == startup_mode_t::disabled);
            }

            /// @brief Reports whether @ref clear leaves this subcommand enabled.
            ///
            /// @return `true` if it is enabled by default.
            [[nodiscard]] auto get_enabled_by_default() const -> bool
            {
                return (default_startup == startup_mode_t::enabled);
            }

            /// @brief Reports whether positionals are validated before assignment.
            ///
            /// @return `true` if they are validated first.
            [[nodiscard]] auto get_validate_positionals() const -> bool
            {
                return validate_positionals_;
            }

            /// @brief Reports whether optional vector arguments are validated before assignment.
            ///
            /// @return `true` if they are validated first.
            [[nodiscard]] auto get_validate_optional_arguments() const -> bool
            {
                return validate_optional_arguments_;
            }

            /// @brief Returns what happens to unmatched configuration entries.
            ///
            /// @return The configuration extras mode.
            [[nodiscard]] auto get_allow_config_extras() const -> config_extras_mode_t
            {
                return allow_config_extras_;
            }

            /// @brief Returns the help flag.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_help_ptr() -> option_t *
            {
                return help_ptr_;
            }

            /// @brief Returns the help flag.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_help_ptr() const -> const option_t *
            {
                return help_ptr_;
            }

            /// @brief Returns the expanded-help flag.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_help_all_ptr() const -> const option_t *
            {
                return help_all_ptr_;
            }

            /// @brief Returns the option naming a configuration file.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_config_ptr() -> option_t *
            {
                return config_ptr_;
            }

            /// @brief Returns the option naming a configuration file.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_config_ptr() const -> const option_t *
            {
                return config_ptr_;
            }

            /// @brief Returns the version flag.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_version_ptr() -> option_t *
            {
                return version_ptr_;
            }

            /// @brief Returns the version flag.
            ///
            /// @return A pointer to the option, or `nullptr` if there is none.
            [[nodiscard]] auto get_version_ptr() const -> const option_t *
            {
                return version_ptr_;
            }

            /// @brief Returns the parent application.
            ///
            /// @return A pointer to the parent, or `nullptr` for the main application.
            [[nodiscard]] auto get_parent() -> app_t *
            {
                return parent_;
            }

            /// @brief Returns the parent application.
            ///
            /// @return A pointer to the parent, or `nullptr` for the main application.
            [[nodiscard]] auto get_parent() const -> const app_t *
            {
                return parent_;
            }

            /// @brief Returns this application's name.
            ///
            /// @return The name.
            [[nodiscard]] auto get_name() const -> const std::string &
            {
                return name_;
            }

            /// @brief Returns the alternative names this subcommand answers to.
            ///
            /// @return The aliases.
            [[nodiscard]] auto get_aliases() const -> const std::vector<std::string> &
            {
                return aliases_;
            }

            /// @brief Removes every alias.
            ///
            /// @return A pointer to this application, for chaining.
            auto clear_aliases() -> app_t *
            {
                aliases_.clear();
                return this;
            }

            /// @brief Returns the name shown in help output.
            ///
            /// @param with_aliases Include the aliases alongside the name.
            /// @return The display name.
            [[nodiscard]] auto get_display_name(bool with_aliases = false) const -> std::string;

            /// @brief Reports whether a name refers to this subcommand.
            ///
            /// Honours the case, underscore, and prefix-matching settings.
            ///
            /// @param name_to_check The name to test.
            /// @return `true` if it matches.
            [[nodiscard]] auto check_name(std::string name_to_check) const -> bool;

            /// @brief How closely a name matched.
            enum class name_match_t : std::uint8_t
            {
                none = 0,  ///< No match.
                exact = 1, ///< The name matched in full.
                prefix = 2 ///< The name matched a prefix, and prefix matching is enabled.
            };

            /// @brief Reports how closely a name matches this subcommand.
            ///
            /// Honours the case and underscore settings.
            ///
            /// @param name_to_check The name to test.
            /// @return How closely it matched.
            [[nodiscard]] auto check_name_detail(std::string name_to_check) const -> name_match_t;

            /// @brief Returns the option groups defined here, in definition order.
            ///
            /// @return The group names.
            [[nodiscard]] auto get_groups() const -> std::vector<std::string>;

            /// @brief Returns the options that were used, in command-line order.
            ///
            /// @return The options, in the order they appeared.
            [[nodiscard]] auto parse_order() const -> const std::vector<option_t *> &
            {
                return parse_order_;
            }

            /// @brief Returns the arguments that matched nothing.
            ///
            /// @param recurse Include the arguments left over by subcommands.
            /// @return The unmatched arguments.
            [[nodiscard]] auto remaining(bool recurse = false) const -> std::vector<std::string>;

            /// @brief Returns the unmatched arguments, ready to hand to another program.
            ///
            /// @param recurse Include the arguments left over by subcommands.
            /// @return The unmatched arguments, in command-line order.
            [[nodiscard]] auto remaining_for_passthrough(bool recurse = false) const -> std::vector<std::string>;

            /// @brief Returns how many arguments matched nothing, excluding the `--` separator.
            ///
            /// @param recurse Include the arguments left over by subcommands.
            /// @return The count.
            [[nodiscard]] auto remaining_size(bool recurse = false) const -> std::size_t;

            ///@}

        protected:
            /// @brief Checks the option set for conflicts.
            ///
            /// Looks for more than one positional taking unlimited arguments, and for
            /// minimum and maximum counts that cannot both be satisfied.
            ///
            /// @throws cli::invalid_error_t If the option set cannot work.
            auto _validate() const -> void;

            /// @brief Prepares the subcommand tree for parsing.
            ///
            /// Sets fallthrough and prefix behaviour on nameless subcommands, applies the
            /// automatic enable or disable, and fixes up parent pointers.
            auto _configure() -> void;

            /// @brief Runs this application's callback, and its subcommands', bottom up.
            ///
            /// @param final_mode Run the final callbacks rather than the parse-complete ones.
            /// @param suppress_final_callback Skip this application's own final callback.
            auto run_callback(bool final_mode = false, bool suppress_final_callback = false) -> void;

            /// @brief Reports whether a token names a usable subcommand.
            ///
            /// Gives up immediately once the subcommand maximum has been reached.
            ///
            /// @param current The token to test.
            /// @param ignore_used Skip subcommands that were already used.
            /// @return `true` if the token names a usable subcommand.
            [[nodiscard]] auto _valid_subcommand(const std::string &current, bool ignore_used = true) const -> bool;

            /// @brief Classifies one command-line argument.
            ///
            /// @param current The argument to classify.
            /// @param ignore_used_subcommands Skip subcommands that were already used.
            /// @return What the argument looks like.
            [[nodiscard]] auto _recognize(const std::string &current,
                                          bool ignore_used_subcommands = true) const -> detail::classifier_t;

            // Parsing is split into the phases below; _process drives them in order.

            /// @brief Reads and applies the configuration file. Main application only.
            auto _process_config_file() -> void;

            /// @brief Reads and applies one configuration file.
            ///
            /// @param config_file The path to read.
            /// @param throw_error Report an error when the file cannot be read.
            /// @return `true` if the file was read.
            auto _process_config_file(const std::string &config_file, bool throw_error) -> bool;

            /// @brief Fills options from environment variables. Runs on every subcommand.
            auto _process_env() -> void;

            /// @brief Runs the option callbacks at one priority. Runs on every subcommand.
            ///
            /// @param priority Which callbacks to run.
            auto _process_callbacks(callback_priority_t priority) -> void;

            /// @brief Handles any help flags that were used.
            ///
            /// The flags let a recursive call remember that a parent already saw one.
            ///
            /// @param priority Which callbacks are being run.
            /// @param trigger_help Whether a parent saw the help flag.
            /// @param trigger_all_help Whether a parent saw the expanded-help flag.
            auto _process_help_flags(callback_priority_t priority, bool trigger_help = false,
                                     bool trigger_all_help = false) const -> void;

            /// @brief Checks required options and cross-requirements, including selected subcommands.
            auto _process_requirements() -> void;

            /// @brief Runs every post-parse phase in order.
            auto _process() -> void;

            /// @brief Reports anything left over that should not be.
            auto _process_extras() -> void;

            /// @brief Increments the parse counter here and on nameless subcommands.
            auto increment_parsed() -> void;

            /// @brief Parses a list of arguments.
            ///
            /// @param[in,out] args The arguments, last one first.
            auto _parse(std::vector<std::string> &args) -> void;

            /// @brief Parses a list of arguments.
            ///
            /// @param args The arguments, last one first.
            auto _parse(std::vector<std::string> &&args) -> void;

            /// @brief Parses a stream as a configuration file.
            ///
            /// @param input The stream to read.
            auto _parse_stream(std::istream &input) -> void;

            /// @brief Applies a set of configuration entries.
            ///
            /// An entry whose name contains a separator is routed into the matching
            /// subcommand.
            ///
            /// @param args The entries to apply.
            auto _parse_config(const std::vector<config_item_t> &args) -> void;

            /// @brief Applies one configuration entry.
            ///
            /// @param item The entry to apply.
            /// @param level How many section levels have been descended.
            /// @return `true` if a matching option was found.
            auto _parse_single_config(const config_item_t &item, std::size_t level = 0) -> bool;

            /// @brief Stores a configuration entry against a flag-like option.
            ///
            /// @param op The option to fill.
            /// @param item The entry being applied.
            /// @param inputs The values from the entry.
            /// @return `true` if the values were stored.
            auto _add_flag_like_result(option_t *op, const config_item_t &item,
                                       const std::vector<std::string> &inputs) -> bool;

            /// @brief Parses one argument, which may consume several.
            ///
            /// Delegates to the parent on failure, and records the argument as missing
            /// if even the main application cannot place it.
            ///
            /// @param[in,out] args The remaining arguments, last one first.
            /// @param[in,out] positional_only Whether the `--` separator has been seen.
            /// @return `false` if parsing failed and should return to the parent.
            auto _parse_single(std::vector<std::string> &args, bool &positional_only) -> bool;

            /// @brief Counts the positionals still waiting for values.
            ///
            /// @param required_only Count only the required ones.
            /// @return The count.
            [[nodiscard]] auto _count_remaining_positionals(bool required_only = false) const -> std::size_t;

            /// @brief Reports whether any positional is still waiting for values.
            ///
            /// @return `true` if one is.
            [[nodiscard]] auto _has_remaining_positionals() const -> bool;

            /// @brief Parses a positional argument, walking up the tree as needed.
            ///
            /// @param[in,out] args The remaining arguments, last one first.
            /// @param[in] halt_on_subcommand Return `false` rather than descending into
            /// a subcommand.
            /// @return `true` if the positional was consumed.
            auto _parse_positional(std::vector<std::string> &args, bool halt_on_subcommand) -> bool;

            /// @brief Finds a subcommand by name.
            ///
            /// @param subc_name The name to look for.
            /// @param ignore_disabled Skip disabled subcommands.
            /// @param ignore_used Skip subcommands that were already used.
            /// @return A pointer to the subcommand, or `nullptr` if none matches.
            [[nodiscard]] auto _find_subcommand(const std::string &subc_name, bool ignore_disabled,
                                                bool ignore_used) const noexcept -> app_t *;

            /// @brief Parses a subcommand and everything it consumes.
            ///
            /// Always allows fallthrough, unlike the other parse helpers.
            ///
            /// @param[in,out] args The remaining arguments, last one first.
            /// @return `true` if a subcommand was processed.
            auto _parse_subcommand(std::vector<std::string> &args) -> bool;

            /// @brief Parses an option argument sitting at the front of the list.
            ///
            /// @param[in,out] args The remaining arguments, last one first.
            /// @param[in] current_type How the leading argument was classified.
            /// @param[in] local_processing_only Disable fallthrough, reporting failure instead.
            /// @return `true` if the argument was consumed.
            auto _parse_arg(std::vector<std::string> &args, detail::classifier_t current_type,
                            bool local_processing_only) -> bool;

            /// @brief Runs the pre-parse callback, if it has not already run.
            ///
            /// @param remaining_args How many arguments are left to parse.
            auto _trigger_pre_parse(std::size_t remaining_args) -> void;

            /// @brief Returns the application to fall through to.
            ///
            /// The nearest named ancestor, or the main application.
            ///
            /// @return A pointer to that application.
            [[nodiscard]] auto _get_fallthrough_parent() noexcept -> app_t *;

            /// @brief Returns the application to fall through to.
            ///
            /// @return A pointer to that application.
            [[nodiscard]] auto _get_fallthrough_parent() const noexcept -> const app_t *;

            /// @brief Returns the first name two subcommands share.
            ///
            /// @param subcom The subcommand being added.
            /// @param base The subcommand to compare against.
            /// @return The colliding name, or an empty string if there is none.
            [[nodiscard]] auto _compare_subcommand_names(const app_t &subcom,
                                                         const app_t &base) const -> const std::string &;

            /// @brief Records an argument that matched nothing.
            ///
            /// @param val_type How the argument was classified.
            /// @param val The argument itself.
            auto _move_to_missing(detail::classifier_t val_type, const std::string &val) -> void;

        public:
            /// @brief Moves an option from one application to another.
            ///
            /// Provided for subclasses that reorganise options into subcommands.
            ///
            /// @param opt The option to move.
            /// @param app The application to move it to.
            auto _move_option(option_t *opt, app_t *app) -> void;
    };

    /// @brief An application specialised for grouping options.
    ///
    /// An option group is a subcommand with no name. It groups options in help
    /// output and can carry its own requirements, but is never named on the command
    /// line. Create one with @ref app_t::add_option_group.
    class option_group_t : public app_t
    {
        public:
            /// @brief Constructs an option group.
            ///
            /// @param group_description The description shown in help output.
            /// @param group_name The group name.
            /// @param parent The owning application.
            option_group_t(std::string group_description, std::string group_name, app_t *parent)
                : app_t(std::move(group_description), "", parent)
            {
                group(group_name);
                // A group with no name, or one marked with a leading '+', is merged into
                // the parent's help output, so it carries no help flags of its own.
                if (group_name.empty() || group_name.front() == '+')
                {
                    set_help_flag("");
                    set_help_all_flag("");
                }
            }

            using app_t::add_option;

            /// @brief Moves an existing option into this group.
            ///
            /// @param opt The option to adopt.
            /// @return @p opt, for chaining.
            /// @throws cli::option_not_found_t If this group has no parent.
            auto add_option(option_t *opt) -> option_t *
            {
                if (get_parent() == nullptr)
                {
                    throw option_not_found_t("Unable to locate the specified option");
                }
                get_parent()->_move_option(opt, this);
                return opt;
            }

            /// @brief Moves an existing option into this group.
            ///
            /// @param opt The option to adopt.
            auto add_options(option_t *opt) -> void
            {
                add_option(opt);
            }

            /// @brief Moves several existing options into this group.
            ///
            /// @param opt The first option to adopt.
            /// @param args The remaining options to adopt.
            template <typename... args_t> auto add_options(option_t *opt, args_t... args) -> void
            {
                add_option(opt);
                add_options(args...);
            }

            using app_t::add_subcommand;

            /// @brief Moves an existing subcommand into this group.
            ///
            /// @param subcom The subcommand to adopt.
            /// @return @p subcom, for chaining.
            auto add_subcommand(app_t *subcom) -> app_t *
            {
                app_ptr_t subc = subcom->get_parent()->get_subcommand_ptr(subcom);
                subc->get_parent()->remove_subcommand(subcom);
                add_subcommand(std::move(subc));
                return subcom;
            }
    };

    /// @brief Enables one subcommand or option group when another is used.
    ///
    /// @param trigger_app The subcommand whose use fires the trigger.
    /// @param app_to_enable The subcommand to enable.
    auto trigger_on(app_t *trigger_app, app_t *app_to_enable) -> void;

    /// @brief Enables several subcommands or option groups when another is used.
    ///
    /// @param trigger_app The subcommand whose use fires the trigger.
    /// @param apps_to_enable The subcommands to enable.
    auto trigger_on(app_t *trigger_app, std::vector<app_t *> apps_to_enable) -> void;

    /// @brief Disables one subcommand or option group when another is used.
    ///
    /// @param trigger_app The subcommand whose use fires the trigger.
    /// @param app_to_enable The subcommand to disable.
    auto trigger_off(app_t *trigger_app, app_t *app_to_enable) -> void;

    /// @brief Disables several subcommands or option groups when another is used.
    ///
    /// @param trigger_app The subcommand whose use fires the trigger.
    /// @param apps_to_enable The subcommands to disable.
    auto trigger_off(app_t *trigger_app, std::vector<app_t *> apps_to_enable) -> void;

    /// @brief Marks an option as deprecated.
    ///
    /// The option keeps working, but its help text records that it should no longer
    /// be used.
    ///
    /// @param opt The option to mark.
    /// @param replacement The option to use instead, if there is one.
    auto deprecate_option(option_t *opt, const std::string &replacement = "") -> void;

    /// @brief Marks a named option as deprecated.
    ///
    /// @param app The application holding the option.
    /// @param option_name The option to mark.
    /// @param replacement The option to use instead, if there is one.
    /// @throws cli::option_not_found_t If no option matches.
    auto deprecate_option(app_t *app, const std::string &option_name,
                          const std::string &replacement = "") -> void
    {
        auto *opt = app->get_option(option_name);
        deprecate_option(opt, replacement);
    }

    /// @brief Marks a named option as deprecated.
    ///
    /// @param app The application holding the option.
    /// @param option_name The option to mark.
    /// @param replacement The option to use instead, if there is one.
    /// @throws cli::option_not_found_t If no option matches.
    auto deprecate_option(app_t &app, const std::string &option_name,
                          const std::string &replacement = "") -> void
    {
        auto *opt = app.get_option(option_name);
        deprecate_option(opt, replacement);
    }

    /// @brief Marks an option as retired.
    ///
    /// A retired option is still accepted on the command line, but does nothing.
    ///
    /// @param app The application holding the option.
    /// @param opt The option to retire.
    auto retire_option(app_t *app, option_t *opt) -> void;

    /// @brief Marks an option as retired.
    ///
    /// @param app The application holding the option.
    /// @param opt The option to retire.
    auto retire_option(app_t &app, option_t *opt) -> void;

    /// @brief Marks a named option as retired.
    ///
    /// If no such option exists, a retired placeholder is added under that name.
    ///
    /// @param app The application holding the option.
    /// @param option_name The option to retire.
    auto retire_option(app_t *app, const std::string &option_name) -> void;

    /// @brief Marks a named option as retired.
    ///
    /// @param app The application holding the option.
    /// @param option_name The option to retire.
    auto retire_option(app_t &app, const std::string &option_name) -> void;

    namespace detail
    {

        /// @brief Grants the test suite access to @ref cli::app_t's protected members.
        struct app_friend_t
        {
                /// @brief Calls @ref cli::app_t::_parse_arg.
                ///
                /// @param app The application to call into.
                /// @param args The arguments to forward.
                /// @return Whatever `_parse_arg` returns.
                template <typename... args_t>
                static auto parse_arg(app_t *app, args_t &&...args) -> decltype(auto)
                {
                    return app->_parse_arg(std::forward<args_t>(args)...);
                }

                /// @brief Calls @ref cli::app_t::_parse_subcommand.
                ///
                /// @param app The application to call into.
                /// @param args The arguments to forward.
                /// @return Whatever `_parse_subcommand` returns.
                template <typename... args_t>
                static auto parse_subcommand(app_t *app, args_t &&...args) -> decltype(auto)
                {
                    return app->_parse_subcommand(std::forward<args_t>(args)...);
                }

                /// @brief Calls @ref cli::app_t::_get_fallthrough_parent.
                ///
                /// @param app The application to call into.
                /// @return The application to fall through to.
                static auto get_fallthrough_parent(app_t *app) -> app_t *
                {
                    return app->_get_fallthrough_parent();
                }

                /// @brief Calls @ref cli::app_t::_get_fallthrough_parent.
                ///
                /// @param app The application to call into.
                /// @return The application to fall through to.
                static auto get_fallthrough_parent(const app_t *app) -> const app_t *
                {
                    return app->_get_fallthrough_parent();
                }
        };

    } // namespace detail

    // =============================================
    // Implementation
    // =============================================

    app_t::app_t(std::string app_description, std::string app_name, app_t *parent)
        : name_(std::move(app_name)), description_(std::move(app_description)), parent_(parent)
    {
        if (parent_ == nullptr)
        {
            return;
        }

        if (parent_->help_ptr_ != nullptr)
        {
            set_help_flag(parent_->help_ptr_->get_name(false, true), parent_->help_ptr_->get_description());
        }
        if (parent_->help_all_ptr_ != nullptr)
        {
            set_help_all_flag(parent_->help_all_ptr_->get_name(false, true),
                              parent_->help_all_ptr_->get_description());
        }

        option_defaults_ = parent_->option_defaults_;

        // Everything below is inheritable; see the member declarations.
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

    auto app_t::ensure_utf8(char **argv) -> char **
    {
#ifdef _WIN32
        (void)argv;

        normalized_argv_ = detail::compute_win32_argv();

        normalized_argv_view_.clear();
        normalized_argv_view_.reserve(normalized_argv_.size());
        for (auto &arg : normalized_argv_)
        {
            normalized_argv_view_.push_back(arg.data());
        }

        return normalized_argv_view_.data();
#else
        return argv;
#endif
    }

    auto app_t::name(std::string app_name) -> app_t *
    {
        if (parent_ != nullptr)
        {
            std::string oname = std::move(name_);
            name_ = app_name;
            const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
            if (!res.empty())
            {
                name_ = std::move(oname);
                throw option_already_added_t(app_name + " conflicts with existing subcommand names");
            }
        }
        else
        {
            name_ = std::move(app_name);
        }
        has_automatic_name_ = false;
        return this;
    }

    auto app_t::alias(std::string app_name) -> app_t *
    {
        if (app_name.empty() || !detail::valid_alias_name_string(app_name))
        {
            throw incorrect_construction_t("Aliases may not be empty or contain newlines or null characters");
        }
        if (parent_ != nullptr)
        {
            aliases_.push_back(app_name);
            const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
            if (!res.empty())
            {
                aliases_.pop_back();
                throw option_already_added_t("alias already matches an existing subcommand: " + app_name);
            }
        }
        else
        {
            aliases_.push_back(std::move(app_name));
        }

        return this;
    }

    auto app_t::immediate_callback(bool immediate) -> app_t *
    {
        immediate_callback_ = immediate;
        // The two callback slots hold the same thing under different names; swap
        // whichever one is occupied into the slot the new mode reads from.
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

    auto app_t::ignore_case(bool value) -> app_t *
    {
        if (value && !ignore_case_)
        {
            ignore_case_ = true;
            auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
            const auto &match = _compare_subcommand_names(*this, *p);
            if (!match.empty())
            {
                // Roll back before throwing, so the object is left as it was found.
                ignore_case_ = false;
                throw option_already_added_t("ignore case would cause subcommand name conflicts: " + match);
            }
        }
        ignore_case_ = value;
        return this;
    }

    auto app_t::ignore_underscore(bool value) -> app_t *
    {
        if (value && !ignore_underscore_)
        {
            ignore_underscore_ = true;
            auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
            const auto &match = _compare_subcommand_names(*this, *p);
            if (!match.empty())
            {
                ignore_underscore_ = false;
                throw option_already_added_t("ignore underscore would cause subcommand name conflicts: " + match);
            }
        }
        ignore_underscore_ = value;
        return this;
    }

    auto app_t::add_option(std::string option_name, callback_t option_callback, std::string option_description,
                           bool defaulted, std::function<std::string()> func) -> option_t *
    {
        option_t myopt {option_name, option_description, option_callback, this, allow_non_standard_options_};

        const auto res =
            std::ranges::find_if(options_, [&myopt](const option_ptr_t &v) { return *v == myopt; });
        if (res != options_.end())
        {
            const auto &matchname = (*res)->matching_name(myopt);
            throw option_already_added_t("added option matched existing option name: " + matchname);
        }

        // Name conflicts have to be checked against the nearest named ancestor, since
        // that is the scope a configuration file addresses.
        const app_t *top_level_parent = this;
        while (top_level_parent->name_.empty() && top_level_parent->parent_ != nullptr)
        {
            top_level_parent = top_level_parent->parent_;
        }

        const auto configurable_conflict = [top_level_parent](const std::string &probe) {
            const auto *op = top_level_parent->get_option_no_throw(probe);
            return op != nullptr && op->get_configurable();
        };

        if (myopt.lnames_.empty() && myopt.snames_.empty())
        {
            // A positional-only option can still collide in a configuration file, where
            // the leading dashes are not written.
            std::string test_name = "--" + myopt.get_single_name();
            if (test_name.size() == 3)
            {
                test_name.erase(0, 1);
            }
            if (configurable_conflict(test_name))
            {
                throw option_already_added_t("added option positional name matches existing option: " + test_name);
            }
            // Two positionals with no dashed names at all cannot be told apart.
            const auto *op = top_level_parent->get_option_no_throw(myopt.get_single_name());
            if (op != nullptr && op->lnames_.empty() && op->snames_.empty())
            {
                throw option_already_added_t("unable to disambiguate with existing option: " + test_name);
            }
        }
        else if (top_level_parent != this)
        {
            for (const auto &ln : myopt.lnames_)
            {
                if (configurable_conflict(ln))
                {
                    throw option_already_added_t("added option matches existing positional option: " + ln);
                }
                if (configurable_conflict("--" + ln))
                {
                    throw option_already_added_t("added option matches existing option: --" + ln);
                }
                if (ln.size() == 1 || top_level_parent->get_allow_non_standard_option_names())
                {
                    if (configurable_conflict("-" + ln))
                    {
                        throw option_already_added_t("added option matches existing option: -" + ln);
                    }
                }
            }
            for (const auto &sn : myopt.snames_)
            {
                if (configurable_conflict(sn))
                {
                    throw option_already_added_t("added option matches existing positional option: " + sn);
                }
                if (configurable_conflict("-" + sn))
                {
                    throw option_already_added_t("added option matches existing option: -" + sn);
                }
                if (configurable_conflict("--" + sn))
                {
                    throw option_already_added_t("added option matches existing option: --" + sn);
                }
            }
        }

        if (allow_non_standard_options_ && !myopt.snames_.empty())
        {
            // A multi-character short name shadows the single-character option sharing
            // its first letter, so neither direction may already exist.
            for (const auto &sname : myopt.snames_)
            {
                if (sname.length() > 1)
                {
                    const std::string test_name {'-', sname.front()};
                    if (top_level_parent->get_option_no_throw(test_name) != nullptr)
                    {
                        throw option_already_added_t("added option interferes with existing short option: " + sname);
                    }
                }
            }
            for (const auto &opt : top_level_parent->get_options())
            {
                for (const auto &osn : opt->snames_)
                {
                    if (osn.size() > 1)
                    {
                        const std::string test_name {osn.front()};
                        if (myopt.check_sname(test_name))
                        {
                            throw option_already_added_t(
                                "added option interferes with existing non standard option: " + osn);
                        }
                    }
                }
            }
        }

        options_.emplace_back();
        option_ptr_t &option = options_.back();
        // Not std::make_unique: option_t's constructor is protected, and app_t reaches
        // it only as a friend.
        option.reset(new option_t(std::move(option_name), std::move(option_description), std::move(option_callback),
                                  this, allow_non_standard_options_));

        option->default_function(std::move(func));

        // Kept for compatibility with CLI11 1.7 and earlier, which captured here.
        if (defaulted)
        {
            option->capture_default_str();
        }

        option_defaults_.copy_to(option.get());

        if (!defaulted && option->get_always_capture_default())
        {
            option->capture_default_str();
        }

        return option.get();
    }

    auto app_t::set_help_flag(std::string flag_name, const std::string &help_description) -> option_t *
    {
        // help_description is taken by const reference so that add_flag does not select
        // the overload that assigns the flag's result back into it.
        if (help_ptr_ != nullptr)
        {
            remove_option(help_ptr_);
            help_ptr_ = nullptr;
        }

        if (!flag_name.empty())
        {
            help_ptr_ = add_flag(std::move(flag_name), help_description);
            help_ptr_->configurable(false)->callback_priority(callback_priority_t::first);
        }

        return help_ptr_;
    }

    auto app_t::set_help_all_flag(std::string help_name, const std::string &help_description) -> option_t *
    {
        if (help_all_ptr_ != nullptr)
        {
            remove_option(help_all_ptr_);
            help_all_ptr_ = nullptr;
        }

        if (!help_name.empty())
        {
            help_all_ptr_ = add_flag(std::move(help_name), help_description);
            help_all_ptr_->configurable(false)->callback_priority(callback_priority_t::first);
        }

        return help_all_ptr_;
    }

    auto app_t::set_version_flag(std::string flag_name, const std::string &version_string,
                                 const std::string &version_help) -> option_t *
    {
        if (version_ptr_ != nullptr)
        {
            remove_option(version_ptr_);
            version_ptr_ = nullptr;
        }

        if (!flag_name.empty())
        {
            version_ptr_ = add_flag_callback(
                std::move(flag_name), [version_string] { throw call_for_version_t(version_string, 0); },
                version_help);
            version_ptr_->configurable(false)->callback_priority(callback_priority_t::first);
        }

        return version_ptr_;
    }

    auto app_t::set_version_flag(std::string flag_name, std::function<std::string()> vfunc,
                                 const std::string &version_help) -> option_t *
    {
        if (version_ptr_ != nullptr)
        {
            remove_option(version_ptr_);
            version_ptr_ = nullptr;
        }

        if (!flag_name.empty())
        {
            version_ptr_ = add_flag_callback(
                std::move(flag_name), [f = std::move(vfunc)] { throw call_for_version_t(f(), 0); }, version_help);
            version_ptr_->configurable(false)->callback_priority(callback_priority_t::first);
        }

        return version_ptr_;
    }

    auto app_t::_add_flag_internal(std::string flag_name, callback_t fun,
                                   std::string flag_description) -> option_t *
    {
        option_t *opt = nullptr;
        if (detail::has_default_flag_values(flag_name))
        {
            auto flag_defaults = detail::get_default_flag_values(flag_name);
            detail::remove_default_flag_values(flag_name);
            opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
            for (const auto &[fname, fdefault] : flag_defaults)
            {
                opt->fnames_.push_back(fname);
            }
            opt->default_flag_values_ = std::move(flag_defaults);
        }
        else
        {
            opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
        }

        // A flag consumes no values, so it cannot also be a positional.
        if (opt->get_positional())
        {
            auto pos_name = opt->get_name(true);
            remove_option(opt);
            throw incorrect_construction_t::positional_flag(pos_name);
        }
        opt->multi_option_policy(multi_option_policy_t::take_last);
        opt->expected(0);
        opt->required(false);
        return opt;
    }

    auto app_t::add_flag_callback(std::string flag_name, std::function<void()> function,
                                  std::string flag_description) -> option_t *
    {
        callback_t fun = [f = std::move(function)](const results_t &res) {
            using detail::lexical_cast;
            bool trigger {false};
            const auto result = lexical_cast(res[0], trigger);
            if (result && trigger)
            {
                f();
            }
            return result;
        };
        return _add_flag_internal(std::move(flag_name), std::move(fun), std::move(flag_description));
    }

    auto app_t::add_flag_function(std::string flag_name, std::function<void(std::int64_t)> function,
                                  std::string flag_description) -> option_t *
    {
        callback_t fun = [f = std::move(function)](const results_t &res) {
            using detail::lexical_cast;
            std::int64_t flag_count {0};
            lexical_cast(res[0], flag_count);
            f(flag_count);
            return true;
        };
        return _add_flag_internal(std::move(flag_name), std::move(fun), std::move(flag_description))
            ->multi_option_policy(multi_option_policy_t::sum);
    }

    auto app_t::set_config(std::string option_name, std::string default_filename, const std::string &help_message,
                           bool config_required) -> option_t *
    {
        if (config_ptr_ != nullptr)
        {
            remove_option(config_ptr_);
            config_ptr_ = nullptr;
        }

        if (!option_name.empty())
        {
            config_ptr_ = add_option(std::move(option_name), help_message);
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
            // Later files take precedence, and are applied in reverse so that the last
            // one named wins.
            config_ptr_->multi_option_policy(multi_option_policy_t::reverse);
        }

        return config_ptr_;
    }

    auto app_t::remove_option(option_t *opt) -> bool
    {
        // Drop any dependency edges pointing at the option before it disappears.
        for (option_ptr_t &op : options_)
        {
            op->remove_needs(opt);
            op->remove_excludes(opt);
        }

        if (help_ptr_ == opt)
        {
            help_ptr_ = nullptr;
        }
        if (help_all_ptr_ == opt)
        {
            help_all_ptr_ = nullptr;
        }
        if (config_ptr_ == opt)
        {
            config_ptr_ = nullptr;
        }

        const auto iterator =
            std::ranges::find_if(options_, [opt](const option_ptr_t &v) { return v.get() == opt; });
        if (iterator != std::end(options_))
        {
            options_.erase(iterator);
            return true;
        }
        return false;
    }

    auto app_t::add_subcommand(std::string subcommand_name, std::string subcommand_description) -> app_t *
    {
        if (!subcommand_name.empty() && !detail::valid_name_string(subcommand_name))
        {
            if (!detail::valid_first_char(subcommand_name[0]))
            {
                throw incorrect_construction_t(
                    "Subcommand name starts with invalid character, '!' and '-' and control characters");
            }
            for (auto c : subcommand_name)
            {
                if (!detail::valid_later_char(c))
                {
                    throw incorrect_construction_t(std::string("Subcommand name contains invalid character ('") + c +
                                                   "'), all characters are allowed except"
                                                   "'=',':','{','}', ' ', and control characters");
                }
            }
        }
        // Not std::make_shared: the three-argument constructor is protected.
        app_ptr_t subcom =
            std::shared_ptr<app_t>(new app_t(std::move(subcommand_description), std::move(subcommand_name), this));
        return add_subcommand(std::move(subcom));
    }

    auto app_t::add_subcommand(app_ptr_t subcom) -> app_t *
    {
        if (!subcom)
        {
            throw incorrect_construction_t("passed app_t is not valid");
        }
        auto *ckapp = (name_.empty() && parent_ != nullptr) ? _get_fallthrough_parent() : this;
        const auto &mstrg = _compare_subcommand_names(*subcom, *ckapp);
        if (!mstrg.empty())
        {
            throw option_already_added_t("subcommand name or alias matches existing subcommand: " + mstrg);
        }
        subcom->parent_ = this;
        subcommands_.push_back(std::move(subcom));
        return subcommands_.back().get();
    }

    auto app_t::remove_subcommand(app_t *subcom) -> bool
    {
        // Drop any dependency edges pointing at the subcommand before it disappears.
        for (app_ptr_t &sub : subcommands_)
        {
            sub->remove_excludes(subcom);
            sub->remove_needs(subcom);
        }

        const auto iterator =
            std::ranges::find_if(subcommands_, [subcom](const app_ptr_t &v) { return v.get() == subcom; });
        if (iterator != std::end(subcommands_))
        {
            subcommands_.erase(iterator);
            return true;
        }
        return false;
    }

    auto app_t::get_subcommand(const app_t *subcom) const -> app_t *
    {
        if (subcom == nullptr)
        {
            throw option_not_found_t("nullptr passed");
        }
        for (const app_ptr_t &subcomptr : subcommands_)
        {
            if (subcomptr.get() == subcom)
            {
                return subcomptr.get();
            }
        }
        throw option_not_found_t(subcom->get_name());
    }

    auto app_t::get_subcommand(std::string subcom) const -> app_t *
    {
        auto *subc = _find_subcommand(subcom, false, false);
        if (subc == nullptr)
        {
            throw option_not_found_t(std::move(subcom));
        }
        return subc;
    }

    auto app_t::get_subcommand_no_throw(std::string subcom) const noexcept -> app_t *
    {
        return _find_subcommand(subcom, false, false);
    }

    auto app_t::get_subcommand(int index) const -> app_t *
    {
        if (index >= 0)
        {
            const auto uindex = static_cast<unsigned>(index);
            if (uindex < subcommands_.size())
            {
                return subcommands_[uindex].get();
            }
        }
        throw option_not_found_t(std::to_string(index));
    }

    auto app_t::get_subcommand_ptr(app_t *subcom) const -> app_ptr_t
    {
        if (subcom == nullptr)
        {
            throw option_not_found_t("nullptr passed");
        }
        for (const app_ptr_t &subcomptr : subcommands_)
        {
            if (subcomptr.get() == subcom)
            {
                return subcomptr;
            }
        }
        throw option_not_found_t(subcom->get_name());
    }

    auto app_t::get_subcommand_ptr(std::string subcom) const -> app_ptr_t
    {
        for (const app_ptr_t &subcomptr : subcommands_)
        {
            if (subcomptr->check_name(subcom))
            {
                return subcomptr;
            }
        }
        throw option_not_found_t(std::move(subcom));
    }

    auto app_t::get_subcommand_ptr(int index) const -> app_ptr_t
    {
        if (index >= 0)
        {
            const auto uindex = static_cast<unsigned>(index);
            if (uindex < subcommands_.size())
            {
                return subcommands_[uindex];
            }
        }
        throw option_not_found_t(std::to_string(index));
    }

    auto app_t::get_option_group(std::string group_name) const -> app_t *
    {
        for (const app_ptr_t &app : subcommands_)
        {
            if (app->name_.empty() && app->group_ == group_name)
            {
                return app.get();
            }
        }
        throw option_not_found_t(std::move(group_name));
    }

    auto app_t::count_all() const -> std::size_t
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
        {
            // A named subcommand also counts each time it was invoked.
            cnt += parsed_;
        }
        return cnt;
    }

    auto app_t::clear() -> void
    {
        parsed_ = 0;
        pre_parse_called_ = false;

        missing_.clear();
        parsed_subcommands_.clear();
        parse_order_.clear();
        for (const option_ptr_t &opt : options_)
        {
            opt->clear();
        }
        for (const app_ptr_t &subc : subcommands_)
        {
            subc->clear();
        }
    }

    auto app_t::parse(int argc, const char *const *argv) -> void
    {
        parse_char_t(argc, argv);
    }

    auto app_t::parse(int argc, const wchar_t *const *argv) -> void
    {
        parse_char_t(argc, argv);
    }

    namespace detail
    {

        /// @brief Passes a narrow string through unchanged.
        ///
        /// @param str The string to pass through.
        /// @return @p str.
        auto maybe_narrow(const char *str) -> const char *
        {
            return str;
        }

        /// @brief Narrows a wide string.
        ///
        /// @param str The string to narrow.
        /// @return The narrowed string.
        auto maybe_narrow(const wchar_t *str) -> std::string
        {
            return narrow(str);
        }

    } // namespace detail

    template <class char_t> auto app_t::parse_char_t(int argc, const char_t *const *argv) -> void
    {
        if (name_.empty() || has_automatic_name_)
        {
            has_automatic_name_ = true;
            name_ = detail::maybe_narrow(argv[0]);
        }

        // Reversed, because parsing consumes from the back.
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc) - 1U);
        for (auto i = static_cast<std::size_t>(argc) - 1U; i > 0U; --i)
        {
            args.emplace_back(detail::maybe_narrow(argv[i]));
        }

        parse(std::move(args));
    }

    auto app_t::parse(std::string commandline, bool program_name_included) -> void
    {
        if (program_name_included)
        {
            auto nstr = detail::split_program_name(commandline);
            if ((name_.empty()) || (has_automatic_name_))
            {
                has_automatic_name_ = true;
                name_ = std::move(nstr.name);
            }
            commandline = std::move(nstr.arguments);
        }
        else
        {
            detail::trim(commandline);
        }

        // Neutralise a separator that only introduces a quoted value, so that
        // `--opt="a b"` survives the split below.
        if (!commandline.empty())
        {
            commandline = detail::find_and_modify(commandline, "=", detail::escape_detect);
            if (allow_windows_style_options_)
            {
                commandline = detail::find_and_modify(commandline, ":", detail::escape_detect);
            }
        }

        auto args = detail::split_up(std::move(commandline));
        std::erase(args, std::string {});
        try
        {
            detail::remove_quotes(args);
        }
        catch (const std::invalid_argument &arg)
        {
            throw parse_error_t(arg.what(), exit_codes_t::invalid_error);
        }
        std::ranges::reverse(args);
        parse(std::move(args));
    }

    auto app_t::parse(std::wstring commandline, bool program_name_included) -> void
    {
        parse(narrow(commandline), program_name_included);
    }

    auto app_t::parse(std::vector<std::string> &args) -> void
    {
        if (parsed_ > 0)
        {
            clear();
        }

        // parsed_ is normally incremented while parsing, but it is set here so that a
        // parse following an error is still cleared, even if _validate or _configure
        // is what threw.
        parsed_ = 1;
        _validate();
        _configure();
        // This object is the root now.
        parent_ = nullptr;
        parsed_ = 0;

        _parse(args);
        run_callback();
    }

    auto app_t::parse(std::vector<std::string> &&args) -> void
    {
        if (parsed_ > 0)
        {
            clear();
        }

        parsed_ = 1;
        _validate();
        _configure();
        parent_ = nullptr;
        parsed_ = 0;

        _parse(std::move(args));
        run_callback();
    }

    auto app_t::parse_from_stream(std::istream &input) -> void
    {
        if (parsed_ == 0)
        {
            _validate();
            _configure();
        }

        _parse_stream(input);
        run_callback();
    }

    auto app_t::exit(const error_t &e, std::ostream &out, std::ostream &err) const -> int
    {
        // Dispatch on the error's own name constant rather than a string literal, so
        // that renaming an error type cannot silently break this.
        if (e.get_name() == runtime_error_t::error_type_name)
        {
            return e.get_exit_code();
        }

        if (e.get_name() == call_for_help_t::error_type_name)
        {
            out << help();
            return e.get_exit_code();
        }

        if (e.get_name() == call_for_all_help_t::error_type_name)
        {
            out << help("", app_format_mode_t::all);
            return e.get_exit_code();
        }

        if (e.get_name() == call_for_version_t::error_type_name)
        {
            out << e.what() << '\n';
            return e.get_exit_code();
        }

        if (e.get_exit_code() != static_cast<int>(exit_codes_t::success))
        {
            if (failure_message_)
            {
                err << failure_message_(this, e) << std::flush;
            }
        }

        return e.get_exit_code();
    }

    auto app_t::get_subcommands(const std::function<bool(const app_t *)> &filter) const
        -> std::vector<const app_t *>
    {
        auto subcomms = subcommands_ | std::views::transform([](const app_ptr_t &v) { return v.get(); }) |
                        std::ranges::to<std::vector<const app_t *>>();

        if (filter)
        {
            std::erase_if(subcomms, [&filter](const app_t *app) { return !filter(app); });
        }

        return subcomms;
    }

    auto app_t::get_subcommands(const std::function<bool(app_t *)> &filter) -> std::vector<app_t *>
    {
        auto subcomms = subcommands_ | std::views::transform([](const app_ptr_t &v) { return v.get(); }) |
                        std::ranges::to<std::vector<app_t *>>();

        if (filter)
        {
            std::erase_if(subcomms, [&filter](app_t *app) { return !filter(app); });
        }

        return subcomms;
    }

    auto app_t::remove_excludes(option_t *opt) -> bool
    {
        return exclude_options_.erase(opt) > 0;
    }

    auto app_t::remove_excludes(app_t *app) -> bool
    {
        if (exclude_subcommands_.erase(app) == 0)
        {
            return false;
        }
        // Subcommand exclusion is symmetric, so drop the other direction too.
        app->remove_excludes(this);
        return true;
    }

    auto app_t::remove_needs(option_t *opt) -> bool
    {
        return need_options_.erase(opt) > 0;
    }

    auto app_t::remove_needs(app_t *app) -> bool
    {
        return need_subcommands_.erase(app) > 0;
    }

    auto app_t::help(std::string prev, app_format_mode_t mode) const -> std::string
    {
        if (prev.empty())
        {
            prev = get_name();
        }
        else
        {
            prev += " " + get_name();
        }

        // Help belongs to the deepest subcommand that was named.
        const auto &selected_subcommands = get_subcommands();
        if (!selected_subcommands.empty())
        {
            return selected_subcommands.back()->help(prev, mode);
        }
        return formatter_->make_help(this, prev, mode);
    }

    auto app_t::version() const -> std::string
    {
        std::string val;
        if (version_ptr_ != nullptr)
        {
            // Running the callback is the only way to reach a user-supplied version
            // function, and it does so by throwing. Save and restore the option's
            // results so that asking for the version does not disturb a parse.
            results_t rv = version_ptr_->results();
            version_ptr_->clear();
            version_ptr_->add_result("true");
            try
            {
                version_ptr_->run_callback();
            }
            catch (const call_for_version_t &cfv)
            {
                val = cfv.what();
            }
            version_ptr_->clear();
            version_ptr_->add_result(rv);
        }
        return val;
    }

    auto app_t::get_options(const std::function<bool(const option_t *)> &filter) const
        -> std::vector<const option_t *>
    {
        auto options = options_ | std::views::transform([](const option_ptr_t &val) { return val.get(); }) |
                       std::ranges::to<std::vector<const option_t *>>();

        if (filter)
        {
            std::erase_if(options, [&filter](const option_t *opt) { return !filter(opt); });
        }

        for (const auto &subcp : subcommands_)
        {
            // Descend into merged option groups.
            const app_t *subc = subcp.get();
            if (subc->get_name().empty() && !subc->get_group().empty() && subc->get_group().front() == '+')
            {
                std::vector<const option_t *> subcopts = subc->get_options(filter);
                options.insert(options.end(), subcopts.begin(), subcopts.end());
            }
        }

        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            const auto *fallthrough_parent = _get_fallthrough_parent();
            std::vector<const option_t *> subcopts = fallthrough_parent->get_options(filter);
            for (const auto *opt : subcopts)
            {
                if (std::ranges::none_of(options, [opt](const option_t *opt2)
                                         { return opt->check_name(opt2->get_name()); }))
                {
                    options.push_back(opt);
                }
            }
        }
        return options;
    }

    auto app_t::get_options(const std::function<bool(option_t *)> &filter) -> std::vector<option_t *>
    {
        auto options = options_ | std::views::transform([](const option_ptr_t &val) { return val.get(); }) |
                       std::ranges::to<std::vector<option_t *>>();

        if (filter)
        {
            std::erase_if(options, [&filter](option_t *opt) { return !filter(opt); });
        }

        for (auto &subc : subcommands_)
        {
            // NOTE: this condition is `||` while the const overload above uses `&&`.
            // Preserved as written; see the accompanying notes.
            if (subc->get_name().empty() || (!subc->get_group().empty() && subc->get_group().front() == '+'))
            {
                auto subcopts = subc->get_options(filter);
                options.insert(options.end(), subcopts.begin(), subcopts.end());
            }
        }

        if (fallthrough_ && parent_ != nullptr && !name_.empty())
        {
            auto *fallthrough_parent = _get_fallthrough_parent();
            std::vector<option_t *> subcopts = fallthrough_parent->get_options(filter);
            for (auto *opt : subcopts)
            {
                if (std::ranges::none_of(options,
                                         [opt](option_t *opt2) { return opt->check_name(opt2->get_name()); }))
                {
                    options.push_back(opt);
                }
            }
        }
        return options;
    }

    auto app_t::get_option(std::string option_name) const -> const option_t *
    {
        const auto *opt = get_option_no_throw(option_name);
        if (opt == nullptr)
        {
            if (fallthrough_ && parent_ != nullptr && name_.empty())
            {
                // An option group with fallthrough may borrow the parent's options. This
                // does not recurse: the call below is to the no-throw form, which does
                // not consult the parent again for an option group.
                return _get_fallthrough_parent()->get_option(option_name);
            }
            throw option_not_found_t(std::move(option_name));
        }
        return opt;
    }

    auto app_t::get_option(std::string option_name) -> option_t *
    {
        auto *opt = get_option_no_throw(option_name);
        if (opt == nullptr)
        {
            if (fallthrough_ && parent_ != nullptr && name_.empty())
            {
                return _get_fallthrough_parent()->get_option(option_name);
            }
            throw option_not_found_t(std::move(option_name));
        }
        return opt;
    }

    auto app_t::get_option_no_throw(std::string option_name) noexcept -> option_t *
    {
        for (option_ptr_t &opt : options_)
        {
            if (opt->check_name(option_name))
            {
                return opt.get();
            }
        }
        for (auto &subc : subcommands_)
        {
            // Descend into option groups, which have no name of their own.
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

    auto app_t::get_option_no_throw(std::string option_name) const noexcept -> const option_t *
    {
        for (const option_ptr_t &opt : options_)
        {
            if (opt->check_name(option_name))
            {
                return opt.get();
            }
        }
        for (const auto &subc : subcommands_)
        {
            // Descend into option groups, which have no name of their own.
            if (subc->get_name().empty())
            {
                const auto *opt = subc->get_option_no_throw(option_name);
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

    auto app_t::get_display_name(bool with_aliases) const -> std::string
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
            dispname.append(", ");
            dispname.append(lalias);
        }
        return dispname;
    }

    auto app_t::check_name(std::string name_to_check) const -> bool
    {
        const auto result = check_name_detail(std::move(name_to_check));
        return (result != name_match_t::none);
    }

    auto app_t::check_name_detail(std::string name_to_check) const -> app_t::name_match_t
    {
        std::string local_name = name_;
        if (ignore_underscore_)
        {
            local_name = detail::remove_underscore(name_);
            name_to_check = detail::remove_underscore(name_to_check);
        }
        if (ignore_case_)
        {
            // NOTE: this reads from name_ rather than from local_name, so when both
            // ignore_underscore_ and ignore_case_ are set the underscore removal above
            // is discarded on this side while still being applied to name_to_check.
            // Preserved as written; see the accompanying notes.
            local_name = detail::to_lower(name_);
            name_to_check = detail::to_lower(name_to_check);
        }

        if (local_name == name_to_check)
        {
            return name_match_t::exact;
        }
        if (allow_prefix_matching_ && name_to_check.size() < local_name.size())
        {
            if (local_name.compare(0, name_to_check.size(), name_to_check) == 0)
            {
                return name_match_t::prefix;
            }
        }

        // Copied deliberately: each alias is folded in place before comparison.
        for (std::string les : aliases_) // NOLINT(performance-for-range-copy)
        {
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
                return name_match_t::exact;
            }
            if (allow_prefix_matching_ && name_to_check.size() < les.size())
            {
                if (les.compare(0, name_to_check.size(), name_to_check) == 0)
                {
                    return name_match_t::prefix;
                }
            }
        }
        return name_match_t::none;
    }

    auto app_t::get_groups() const -> std::vector<std::string>
    {
        std::vector<std::string> groups;

        for (const option_ptr_t &opt : options_)
        {
            if (!std::ranges::contains(groups, opt->get_group()))
            {
                groups.push_back(opt->get_group());
            }
        }

        return groups;
    }

    auto app_t::remaining(bool recurse) const -> std::vector<std::string>
    {
        std::vector<std::string> miss_list;
        miss_list.reserve(missing_.size());
        for (const auto &[classification, value] : missing_)
        {
            miss_list.push_back(value);
        }

        if (recurse)
        {
            // An option group cannot report its own extras, so collect them here when
            // this application is not itself capturing them.
            if (allow_extras_ == extras_mode_t::error || allow_extras_ == extras_mode_t::ignore)
            {
                for (const auto &sub : subcommands_)
                {
                    if (sub->name_.empty() && !sub->missing_.empty())
                    {
                        for (const auto &[classification, value] : sub->missing_)
                        {
                            miss_list.push_back(value);
                        }
                    }
                }
            }

            for (const app_t *sub : parsed_subcommands_)
            {
                const std::vector<std::string> output = sub->remaining(recurse);
                miss_list.insert(miss_list.end(), output.begin(), output.end());
            }
        }
        return miss_list;
    }

    auto app_t::remaining_for_passthrough(bool recurse) const -> std::vector<std::string>
    {
        std::vector<std::string> miss_list = remaining(recurse);
        std::ranges::reverse(miss_list);
        return miss_list;
    }

    auto app_t::remaining_size(bool recurse) const -> std::size_t
    {
        auto remaining_options = static_cast<std::size_t>(
            std::ranges::count_if(missing_, [](const std::pair<detail::classifier_t, std::string> &val)
                                  { return val.first != detail::classifier_t::positional_mark; }));

        if (recurse)
        {
            for (const app_ptr_t &sub : subcommands_)
            {
                remaining_options += sub->remaining_size(recurse);
            }
        }
        return remaining_options;
    }

    auto app_t::_validate() const -> void
    {
        // More than one unbounded positional cannot be told apart, unless all but one
        // are required and therefore consume a fixed share.
        const auto pcount = std::ranges::count_if(options_, [](const option_ptr_t &opt) {
            return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional();
        });
        if (pcount > 1)
        {
            const auto pcount_req = std::ranges::count_if(options_, [](const option_ptr_t &opt) {
                return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional() &&
                       opt->get_required();
            });
            if (pcount - pcount_req > 1)
            {
                throw invalid_error_t(name_);
            }
        }

        std::size_t nameless_subs {0};
        for (const app_ptr_t &app : subcommands_)
        {
            app->_validate();
            if (app->get_name().empty())
            {
                ++nameless_subs;
            }
        }

        if (require_option_min_ > 0)
        {
            if (require_option_max_ > 0)
            {
                if (require_option_max_ < require_option_min_)
                {
                    throw invalid_error_t("Required min options greater than required max options",
                                          exit_codes_t::invalid_error);
                }
            }
            if (require_option_min_ > (options_.size() + nameless_subs))
            {
                throw invalid_error_t("Required min options greater than number of available options",
                                      exit_codes_t::invalid_error);
            }
        }
    }

    auto app_t::_configure() -> void
    {
        if (default_startup == startup_mode_t::enabled)
        {
            disabled_ = false;
        }
        else if (default_startup == startup_mode_t::disabled)
        {
            disabled_ = true;
        }

        for (const app_ptr_t &app : subcommands_)
        {
            if (app->has_automatic_name_)
            {
                app->name_.clear();
            }
            if (app->name_.empty())
            {
                // An unnamed subcommand must not fall through, or lookups would cycle
                // between it and its parent.
                app->fallthrough_ = false;
                app->prefix_command_ = prefix_command_mode_t::off;
            }
            app->parent_ = this;
            app->_configure();
        }
    }

    auto app_t::run_callback(bool final_mode, bool suppress_final_callback) -> void
    {
        pre_callback();

        // With immediate_callback_ set, the parse-complete callback runs ahead of the
        // subcommands rather than after them.
        if (!final_mode && parse_complete_callback_)
        {
            parse_complete_callback_();
        }

        for (app_t *subc : get_subcommands())
        {
            if (subc->parent_ == this)
            {
                subc->run_callback(true, suppress_final_callback);
            }
        }

        for (auto &subc : subcommands_)
        {
            if (subc->name_.empty() && subc->count_all() > 0)
            {
                subc->run_callback(true, suppress_final_callback);
            }
        }

        if (final_callback_ && (parsed_ > 0) && (!suppress_final_callback))
        {
            if (!name_.empty() || count_all() > 0 || parent_ == nullptr)
            {
                final_callback_();
            }
        }
    }

    auto app_t::_valid_subcommand(const std::string &current, bool ignore_used) const -> bool
    {
        // Once the subcommand maximum is reached this application stops matching, but a
        // parent may still be willing to.
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
        if (subcommand_fallthrough_)
        {
            return parent_ != nullptr && parent_->_valid_subcommand(current, ignore_used);
        }
        return false;
    }

    auto app_t::_recognize(const std::string &current, bool ignore_used_subcommands) const -> detail::classifier_t
    {
        if (current == "--")
        {
            return detail::classifier_t::positional_mark;
        }
        if (_valid_subcommand(current, ignore_used_subcommands))
        {
            return detail::classifier_t::subcommand;
        }
        if (detail::split_long(current))
        {
            return detail::classifier_t::long_;
        }
        if (const auto res = detail::split_short(current))
        {
            const auto &[sname, rest] = *res;
            if ((sname[0] >= '0' && sname[0] <= '9') ||
                (sname[0] == '.' && !rest.empty() && (rest[0] >= '0' && rest[0] <= '9')))
            {
                // Looks like a negative number rather than an option, unless an option
                // with that single-character name actually exists.
                if (get_option_no_throw(std::string {'-', sname[0]}) == nullptr)
                {
                    return detail::classifier_t::none;
                }
            }
            return detail::classifier_t::short_;
        }
        if (allow_windows_style_options_ && detail::split_windows_style(current))
        {
            return detail::classifier_t::windows_style;
        }
        if ((current == "++") && !name_.empty() && parent_ != nullptr)
        {
            return detail::classifier_t::subcommand_terminator;
        }

        // A dotted token may address a subcommand's subcommand.
        const auto dotloc = current.find_first_of('.');
        if (dotloc != std::string::npos)
        {
            auto *cm = _find_subcommand(current.substr(0, dotloc), true, ignore_used_subcommands);
            if (cm != nullptr)
            {
                const auto res = cm->_recognize(current.substr(dotloc + 1), ignore_used_subcommands);
                if (res == detail::classifier_t::subcommand)
                {
                    return res;
                }
            }
        }
        return detail::classifier_t::none;
    }

    auto app_t::_process_config_file(const std::string &config_file, bool throw_error) -> bool
    {
        const auto path_result = detail::check_path(config_file);
        if (path_result != detail::path_type_t::file)
        {
            if (throw_error)
            {
                throw file_error_t::missing(config_file);
            }
            return false;
        }

        try
        {
            const std::vector<config_item_t> values = config_formatter_->from_file(config_file);
            _parse_config(values);
            return true;
        }
        catch (const file_error_t &)
        {
            if (throw_error)
            {
                throw;
            }
            return false;
        }
    }

    auto app_t::_process_config_file() -> void
    {
        if (config_ptr_ == nullptr)
        {
            return;
        }

        const bool config_required = config_ptr_->get_required();
        const auto file_given = config_ptr_->count() > 0;
        if (!(file_given || config_ptr_->envname_.empty()))
        {
            std::string ename_string = detail::get_environment_value(config_ptr_->envname_);
            if (!ename_string.empty())
            {
                config_ptr_->add_result(std::move(ename_string));
            }
        }
        config_ptr_->run_callback();

        const auto config_files = config_ptr_->as<std::vector<std::string>>();
        bool files_used {file_given};
        if (config_files.empty() || config_files.front().empty())
        {
            if (config_required)
            {
                throw file_error_t("config file is required but none was given");
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
            // Reset so that the option reports a count of zero when nothing was read.
            config_ptr_->clear();
            const bool force = config_ptr_->force_callback_;
            config_ptr_->force_callback_ = false;
            config_ptr_->run_callback();
            config_ptr_->force_callback_ = force;
        }
    }

    auto app_t::_process_env() -> void
    {
        for (const option_ptr_t &opt : options_)
        {
            if (opt->count() == 0 && !opt->envname_.empty())
            {
                std::string ename_string = detail::get_environment_value(opt->envname_);
                if (!ename_string.empty())
                {
                    // _validate may rewrite its argument, so it gets a copy; the value
                    // actually stored is the untouched environment string.
                    std::string candidate = ename_string;
                    const std::string err = opt->_validate(candidate, 0);
                    if (err.empty())
                    {
                        opt->add_result(std::move(ename_string));
                    }
                }
            }
        }

        for (app_ptr_t &sub : subcommands_)
        {
            // Only descend once a subcommand's own callback has already fired.
            if (sub->get_name().empty() || (sub->count_all() > 0 && !sub->parse_complete_callback_))
            {
                sub->_process_env();
            }
        }
    }

    auto app_t::_process_callbacks(callback_priority_t priority) -> void
    {
        for (app_ptr_t &sub : subcommands_)
        {
            // Option groups carrying their own callback go first.
            if (sub->get_name().empty() && sub->parse_complete_callback_)
            {
                if (sub->count_all() > 0)
                {
                    sub->_process_callbacks(priority);
                    if (priority == callback_priority_t::normal)
                    {
                        sub->run_callback();
                    }
                }
            }
        }

        for (const option_ptr_t &opt : options_)
        {
            if (opt->get_callback_priority() == priority)
            {
                if ((*opt) && !opt->get_callback_run())
                {
                    opt->run_callback();
                }
            }
        }

        for (app_ptr_t &sub : subcommands_)
        {
            if (!sub->parse_complete_callback_)
            {
                sub->_process_callbacks(priority);
            }
        }
    }

    auto app_t::_process_help_flags(callback_priority_t priority, bool trigger_help,
                                    bool trigger_all_help) const -> void
    {
        const option_t *help_ptr = get_help_ptr();
        const option_t *help_all_ptr = get_help_all_ptr();

        if (help_ptr != nullptr && help_ptr->count() > 0 && help_ptr->get_callback_priority() == priority)
        {
            trigger_help = true;
        }
        if (help_all_ptr != nullptr && help_all_ptr->count() > 0 && help_all_ptr->get_callback_priority() == priority)
        {
            trigger_all_help = true;
        }

        if (!parsed_subcommands_.empty())
        {
            // Only the deepest subcommand actually calls for help; the flags carry the
            // request down to it.
            for (const app_t *sub : parsed_subcommands_)
            {
                sub->_process_help_flags(priority, trigger_help, trigger_all_help);
            }
        }
        else if (trigger_all_help)
        {
            // Expanded help wins over ordinary help.
            throw call_for_all_help_t();
        }
        else if (trigger_help)
        {
            throw call_for_help_t();
        }
    }

    auto app_t::_process_requirements() -> void
    {
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
                throw excludes_error_t(get_display_name(), excluder);
            }
            // Excluded, but unused, so there is nothing to conflict with.
            return;
        }

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
                throw requires_error_t(get_display_name(), missing_need);
            }
            // A dependency is unmet, but nothing here was used, so it does not matter.
            return;
        }

        std::size_t used_options = 0;
        for (const option_ptr_t &opt : options_)
        {
            if (opt->count() != 0)
            {
                ++used_options;
            }
            if (opt->get_required() && opt->count() == 0)
            {
                throw required_error_t(opt->get_name());
            }
            for (const option_t *opt_req : opt->needs_)
            {
                if (opt->count() > 0 && opt_req->count() == 0)
                {
                    throw requires_error_t(opt->get_name(), opt_req->get_name());
                }
            }
            for (const option_t *opt_ex : opt->excludes_)
            {
                if (opt->count() > 0 && opt_ex->count() != 0)
                {
                    throw excludes_error_t(opt->get_name(), opt_ex->get_name());
                }
            }
        }

        if (require_subcommand_min_ > 0)
        {
            const auto &selected_subcommands = get_subcommands();
            if (require_subcommand_min_ > selected_subcommands.size())
            {
                throw required_error_t::subcommand(require_subcommand_min_);
            }
        }

        // There is no maximum check here: a surplus subcommand surfaces later as an
        // extras error or as a remaining argument.

        // An unnamed subcommand counts as an option from this application's point of
        // view, so fold the used ones into the option tally.
        for (app_ptr_t &sub : subcommands_)
        {
            if (sub->disabled_)
            {
                continue;
            }
            if (sub->name_.empty() && sub->count_all() > 0)
            {
                ++used_options;
            }
        }

        if (require_option_min_ > used_options || (require_option_max_ > 0 && require_option_max_ < used_options))
        {
            auto option_list = detail::join(options_, [this](const option_ptr_t &ptr) {
                if (ptr.get() == help_ptr_ || ptr.get() == help_all_ptr_)
                {
                    return std::string {};
                }
                return ptr->get_name(false, true);
            });

            const auto subc_list =
                get_subcommands([](app_t *app) { return ((app->get_name().empty()) && (!app->disabled_)); });
            if (!subc_list.empty())
            {
                option_list += "," + detail::join(subc_list, [](const app_t *app) { return app->get_display_name(); });
            }
            throw required_error_t::option(require_option_min_, require_option_max_, used_options, option_list);
        }

        for (app_ptr_t &sub : subcommands_)
        {
            if (sub->disabled_)
            {
                continue;
            }
            if (sub->name_.empty() && !sub->required_)
            {
                // An empty, optional option group is skipped once the enclosing
                // requirement is already satisfied.
                if (sub->count_all() == 0)
                {
                    if (require_option_min_ > 0 && require_option_min_ <= used_options)
                    {
                        continue;
                    }
                    if (require_option_max_ > 0 && used_options >= require_option_min_)
                    {
                        continue;
                    }
                }
            }
            if (sub->count() > 0 || sub->name_.empty())
            {
                sub->_process_requirements();
            }

            if (sub->required_ && sub->count_all() == 0)
            {
                throw required_error_t(sub->get_display_name());
            }
        }
    }

    auto app_t::_process() -> void
    {
        // Help outranks every other error, and neither the configuration file nor the
        // environment should be consulted if help is going to be thrown.
        _process_callbacks(callback_priority_t::first_pre_help);
        _process_help_flags(callback_priority_t::first);
        _process_callbacks(callback_priority_t::first);

        std::exception_ptr config_exception;
        try
        {
            // A missing configuration file is held back so that help, version, and
            // other errors get a chance to surface first.
            _process_config_file();

            // Reading the environment should not throw, but there is no point doing it
            // if the configuration file already failed.
            _process_env();
        }
        catch (const file_error_t &)
        {
            config_exception = std::current_exception();
        }

        // Callback and requirement errors outrank the held-back configuration error.
        _process_callbacks(callback_priority_t::pre_requirements_check_pre_help);
        _process_help_flags(callback_priority_t::pre_requirements_check);
        _process_callbacks(callback_priority_t::pre_requirements_check);

        _process_requirements();

        _process_callbacks(callback_priority_t::normal_pre_help);
        _process_help_flags(callback_priority_t::normal);
        _process_callbacks(callback_priority_t::normal);

        if (config_exception)
        {
            std::rethrow_exception(config_exception);
        }

        _process_callbacks(callback_priority_t::last_pre_help);
        _process_help_flags(callback_priority_t::last);
        _process_callbacks(callback_priority_t::last);
    }

    auto app_t::_process_extras() -> void
    {
        if (allow_extras_ == extras_mode_t::error && prefix_command_ == prefix_command_mode_t::off)
        {
            if (remaining_size() > 0)
            {
                throw extras_error_t(name_, remaining(false));
            }
        }
        if (allow_extras_ == extras_mode_t::error && prefix_command_ == prefix_command_mode_t::separator_only)
        {
            if (remaining_size() > 0)
            {
                // In separator-only mode a leading `--` is the sanctioned way to pass
                // arguments through, so only anything else is an error.
                auto left_over = remaining(false);
                if (left_over.front() != "--")
                {
                    throw extras_error_t(name_, std::move(left_over));
                }
            }
        }
        for (app_ptr_t &sub : subcommands_)
        {
            if (sub->count() > 0)
            {
                sub->_process_extras();
            }
        }
    }

    auto app_t::increment_parsed() -> void
    {
        ++parsed_;
        for (app_ptr_t &sub : subcommands_)
        {
            if (sub->get_name().empty())
            {
                sub->increment_parsed();
            }
        }
    }

    auto app_t::_parse(std::vector<std::string> &args) -> void
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
            _process_extras();
            // Reduce the classified leftovers to plain strings, ready for another parser.
            args = remaining_for_passthrough(false);
        }
        else if (parse_complete_callback_)
        {
            // The same sequence as _process, minus the configuration file, which only
            // the root application reads.
            _process_callbacks(callback_priority_t::first_pre_help);
            _process_help_flags(callback_priority_t::first);
            _process_callbacks(callback_priority_t::first);
            _process_env();
            _process_callbacks(callback_priority_t::pre_requirements_check_pre_help);
            _process_help_flags(callback_priority_t::pre_requirements_check);
            _process_callbacks(callback_priority_t::pre_requirements_check);
            _process_requirements();
            _process_callbacks(callback_priority_t::normal_pre_help);
            _process_help_flags(callback_priority_t::normal);
            _process_callbacks(callback_priority_t::normal);
            _process_callbacks(callback_priority_t::last_pre_help);
            _process_help_flags(callback_priority_t::last);
            _process_callbacks(callback_priority_t::last);
            run_callback(false, true);
        }
    }

    auto app_t::_parse(std::vector<std::string> &&args) -> void
    {
        // Only reachable from the top level, where parent_ is null by definition, so
        // the fallthrough handling above is not needed.
        increment_parsed();
        _trigger_pre_parse(args.size());
        bool positional_only = false;

        while (!args.empty())
        {
            _parse_single(args, positional_only);
        }
        _process();
        _process_extras();
    }

    auto app_t::_parse_stream(std::istream &input) -> void
    {
        const auto values = config_formatter_->from_config(input);
        _parse_config(values);
        increment_parsed();
        _trigger_pre_parse(values.size());
        _process();
        _process_extras();
    }

    auto app_t::_parse_config(const std::vector<config_item_t> &args) -> void
    {
        for (const config_item_t &item : args)
        {
            if (!_parse_single_config(item) && allow_config_extras_ == config_extras_mode_t::error)
            {
                throw config_error_t::extras(item.fullname());
            }
        }
    }

    auto app_t::_add_flag_like_result(option_t *op, const config_item_t &item,
                                      const std::vector<std::string> &inputs) -> bool
    {
        if (item.inputs.size() <= 1)
        {
            auto res = config_formatter_->to_flag(item);
            bool converted {false};
            if (op->get_disable_flag_override())
            {
                const auto val = detail::to_flag_value(res);
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

            op->add_result(std::move(res));
            return true;
        }

        if (static_cast<int>(inputs.size()) > op->get_items_expected_max() &&
            op->get_multi_option_policy() != multi_option_policy_t::take_all &&
            op->get_multi_option_policy() != multi_option_policy_t::join)
        {
            if (op->get_items_expected_max() > 1)
            {
                throw argument_mismatch_t::at_most(item.fullname(), op->get_items_expected_max(), inputs.size());
            }

            if (!op->get_disable_flag_override())
            {
                throw conversion_error_t::too_many_inputs_flag(item.fullname());
            }

            // With flag overrides disabled every value must be one the flag recognises,
            // whatever the resulting output, so an array of them is still acceptable.
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
                    valid_value = std::ranges::any_of(op->default_flag_values_,
                                                      [&res](const auto &valid_res) { return valid_res.second == res; });
                }

                if (valid_value)
                {
                    op->add_result(res);
                }
                else
                {
                    throw invalid_error_t("invalid flag argument given");
                }
            }
            return true;
        }
        return false;
    }

    auto app_t::_parse_single_config(const config_item_t &item, std::size_t level) -> bool
    {
        if (level < item.parents.size())
        {
            auto *subcom = get_subcommand_no_throw(item.parents.at(level));
            return (subcom != nullptr) ? subcom->_parse_single_config(item, level + 1) : false;
        }

        // A "++" entry marks a section opening.
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

        // A "--" entry marks a section closing.
        if (item.name == "--")
        {
            if (configurable_ && parse_complete_callback_)
            {
                // As in _parse, but without the help flags: a configuration file cannot
                // ask for help.
                _process_callbacks(callback_priority_t::first_pre_help);
                _process_callbacks(callback_priority_t::first);
                _process_callbacks(callback_priority_t::pre_requirements_check_pre_help);
                _process_callbacks(callback_priority_t::pre_requirements_check);
                _process_requirements();
                _process_callbacks(callback_priority_t::normal_pre_help);
                _process_callbacks(callback_priority_t::normal);
                _process_callbacks(callback_priority_t::last_pre_help);
                _process_callbacks(callback_priority_t::last);
                run_callback();
            }
            return true;
        }

        // Configuration names carry no dashes, so try each spelling in turn.
        option_t *op = get_option_no_throw("--" + item.name);
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
            // The long spelling matched something that cannot be configured; a short
            // option of the same name might still be usable.
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
            const auto options = get_options([&name = item.name](const option_t *opt) {
                return (opt->get_configurable() &&
                        (opt->check_name(name) || opt->check_lname(name) || opt->check_sname(name)));
            });
            if (!options.empty())
            {
                op = options[0];
            }
        }

        if (op == nullptr)
        {
            if (get_allow_config_extras() == config_extras_mode_t::capture)
            {
                // Recorded unclassified; a configuration entry has no command-line form.
                missing_.emplace_back(detail::classifier_t::none, item.fullname());
                for (const auto &input : item.inputs)
                {
                    missing_.emplace_back(detail::classifier_t::none, input);
                }
            }
            return false;
        }

        if (!op->get_configurable())
        {
            if (get_allow_config_extras() == config_extras_mode_t::ignore_all)
            {
                return false;
            }
            throw config_error_t::not_configurable(item.fullname());
        }

        if (op->empty())
        {
            std::vector<std::string> buffer;
            bool use_buffer {false};
            if (item.multiline)
            {
                // The separator only matters to an option that asked for one.
                if (!op->get_inject_separator())
                {
                    buffer = item.inputs;
                    std::erase(buffer, "%%");
                    use_buffer = true;
                }
            }
            const std::vector<std::string> &inputs = (use_buffer) ? buffer : item.inputs;
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

    auto app_t::_parse_single(std::vector<std::string> &args, bool &positional_only) -> bool
    {
        bool retval = true;
        const detail::classifier_t classifier =
            positional_only ? detail::classifier_t::none : _recognize(args.back());

        switch (classifier)
        {
        case detail::classifier_t::positional_mark:
            args.pop_back();
            positional_only = true;
            if (get_prefix_command())
            {
                // In prefix-command mode everything after the marker is handed straight
                // back, whatever the extras mode says.
                missing_.emplace_back(classifier, "--");
                while (!args.empty())
                {
                    missing_.emplace_back(detail::classifier_t::none, args.back());
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

        case detail::classifier_t::subcommand_terminator:
            // Behaves like a positional marker, but in the parent application.
            args.pop_back();
            retval = false;
            break;

        case detail::classifier_t::subcommand:
            retval = _parse_subcommand(args);
            break;

        case detail::classifier_t::long_:
        case detail::classifier_t::short_:
        case detail::classifier_t::windows_style:
            retval = _parse_arg(args, classifier, false);
            break;

        case detail::classifier_t::none:
            // A positional, or something belonging to a parent command.
            retval = _parse_positional(args, false);
            if (retval && positionals_at_end_)
            {
                positional_only = true;
            }
            break;

            // LCOV_EXCL_START
        default:
            throw horrible_error_t("unrecognized classifier (you should not see this!)");
            // LCOV_EXCL_STOP
        }
        return retval;
    }

    auto app_t::_count_remaining_positionals(bool required_only) const -> std::size_t
    {
        std::size_t retval = 0;
        for (const option_ptr_t &opt : options_)
        {
            if (opt->get_positional() && (!required_only || opt->get_required()))
            {
                if (opt->get_items_expected_min() > 0 &&
                    static_cast<int>(opt->count()) < opt->get_items_expected_min())
                {
                    retval += static_cast<std::size_t>(opt->get_items_expected_min()) - opt->count();
                }
            }
        }
        return retval;
    }

    auto app_t::_has_remaining_positionals() const -> bool
    {
        return std::ranges::any_of(options_, [](const option_ptr_t &opt) {
            return opt->get_positional() && (static_cast<int>(opt->count()) < opt->get_items_expected_min());
        });
    }

    auto app_t::_parse_positional(std::vector<std::string> &args, bool halt_on_subcommand) -> bool
    {
        const std::string &positional = args.back();
        option_t *pos_opt {nullptr};

        // With validate_positionals_ set, an option that would reject this value is
        // passed over so that a later positional can take it.
        const auto rejects_positional = [this, &positional](const option_ptr_t &opt) {
            if (!validate_positionals_)
            {
                return false;
            }
            std::string pos = positional;
            return !opt->_validate(pos, 0).empty();
        };

        if (positionals_at_end_)
        {
            // Once only as many arguments remain as there are required positionals, the
            // required ones take precedence over anything optional.
            const auto arg_rem = args.size();
            const auto remreq = _count_remaining_positionals(true);
            if (arg_rem <= remreq)
            {
                for (const option_ptr_t &opt : options_)
                {
                    if (opt->get_positional() && opt->required_)
                    {
                        if (static_cast<int>(opt->count()) < opt->get_items_expected_min())
                        {
                            if (rejects_positional(opt))
                            {
                                continue;
                            }
                            pos_opt = opt.get();
                            break;
                        }
                    }
                }
            }
        }

        if (pos_opt == nullptr)
        {
            // Fill the positionals in declaration order, one at a time.
            for (const option_ptr_t &opt : options_)
            {
                if (opt->get_positional() &&
                    (static_cast<int>(opt->count()) < opt->get_items_expected_max() || opt->get_allow_extra_args()))
                {
                    if (rejects_positional(opt))
                    {
                        continue;
                    }
                    pos_opt = opt.get();
                    break;
                }
            }
        }

        if (pos_opt != nullptr)
        {
            parse_order_.push_back(pos_opt);
            if (pos_opt->get_inject_separator())
            {
                if (!pos_opt->results().empty() && !pos_opt->results().back().empty())
                {
                    pos_opt->add_result(std::string {});
                }
            }

            results_t prev;
            if (pos_opt->get_trigger_on_parse() &&
                pos_opt->current_option_state_ == option_t::option_state_t::callback_run)
            {
                prev = pos_opt->results();
                pos_opt->clear();
            }

            if (pos_opt->get_expected_min() == 0)
            {
                config_item_t item;
                item.name = pos_opt->pname_;
                item.inputs.push_back(positional);
                // A single input always succeeds, so the result is not checked.
                _add_flag_like_result(pos_opt, item, item.inputs);
            }
            else
            {
                pos_opt->add_result(positional);
            }

            if (pos_opt->get_trigger_on_parse())
            {
                if (!pos_opt->empty())
                {
                    pos_opt->run_callback();
                }
                else if (!prev.empty())
                {
                    pos_opt->add_result(prev);
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

        if (parent_ != nullptr && fallthrough_)
        {
            return _get_fallthrough_parent()->_parse_positional(args, static_cast<bool>(parse_complete_callback_));
        }

        // A subcommand may be repeated, so check for one locally before giving up.
        auto *com = _find_subcommand(args.back(), true, false);
        if (com != nullptr && (require_subcommand_max_ == 0 || require_subcommand_max_ > parsed_subcommands_.size()))
        {
            if (halt_on_subcommand)
            {
                return false;
            }
            args.pop_back();
            com->_parse(args);
            return true;
        }

        if (subcommand_fallthrough_)
        {
            // Last chance: search from the root for a subcommand that ran earlier, and
            // if one exists let the parent take the argument.
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
            const std::vector<std::string> rargs(args.rbegin(), args.rend());
            throw extras_error_t(name_, rargs);
        }

        // An option group leaves the decision to its parent.
        if (parent_ != nullptr && name_.empty())
        {
            return false;
        }

        _move_to_missing(detail::classifier_t::none, positional);
        args.pop_back();
        if (get_prefix_command())
        {
            while (!args.empty())
            {
                missing_.emplace_back(detail::classifier_t::none, args.back());
                args.pop_back();
            }
        }

        return true;
    }

    auto app_t::_find_subcommand(const std::string &subc_name, bool ignore_disabled,
                                 bool ignore_used) const noexcept -> app_t *
    {
        app_t *bcom {nullptr};
        for (const app_ptr_t &com : subcommands_)
        {
            if (com->disabled_ && ignore_disabled)
            {
                continue;
            }
            if (com->get_name().empty())
            {
                // Descend into option groups, which hold subcommands but have no name.
                auto *subc = com->_find_subcommand(subc_name, ignore_disabled, ignore_used);
                if (subc != nullptr)
                {
                    if (bcom != nullptr)
                    {
                        // Two prefix matches is an ambiguity, so neither is returned.
                        return nullptr;
                    }
                    bcom = subc;
                    if (!allow_prefix_matching_)
                    {
                        return bcom;
                    }
                }
            }

            const auto res = com->check_name_detail(subc_name);
            if (res != app_t::name_match_t::none)
            {
                if ((!*com) || !ignore_used)
                {
                    // An exact match wins immediately; a prefix match has to wait, in
                    // case a second one turns up and makes it ambiguous.
                    if (res == app_t::name_match_t::exact)
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

    auto app_t::_parse_subcommand(std::vector<std::string> &args) -> bool
    {
        if (_count_remaining_positionals(true) > 0)
        {
            _parse_positional(args, false);
            return true;
        }

        auto *com = _find_subcommand(args.back(), true, true);
        if (com == nullptr)
        {
            // Reached mainly through dotted notation, where `sub.opt` addresses a
            // subcommand's option directly.
            const auto dotloc = args.back().find_first_of('.');
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

            // Record the subcommand on every application between it and this one.
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
        {
            throw horrible_error_t("Subcommand " + args.back() + " missing");
        }
        return false;
    }

    auto app_t::_parse_arg(std::vector<std::string> &args, detail::classifier_t current_type,
                           bool local_processing_only) -> bool
    {
        std::string current = args.back();

        std::string arg_name;
        std::string value;
        std::string rest;

        switch (current_type)
        {
        case detail::classifier_t::long_:
        {
            const auto res = detail::split_long(current);
            if (!res)
            {
                throw horrible_error_t("Long parsed but missing (you should not see this):" + args.back());
            }
            arg_name = res->name;
            value = res->value;
        }
        break;

        case detail::classifier_t::short_:
        {
            const auto res = detail::split_short(current);
            if (!res)
            {
                throw horrible_error_t("Short parsed but missing! You should not see this");
            }
            arg_name = res->name;
            rest = res->value;
        }
        break;

        case detail::classifier_t::windows_style:
        {
            const auto res = detail::split_windows_style(current);
            if (!res)
            {
                throw horrible_error_t("windows option parsed but missing! You should not see this");
            }
            arg_name = res->name;
            value = res->value;
        }
        break;

        case detail::classifier_t::subcommand:
        case detail::classifier_t::subcommand_terminator:
        case detail::classifier_t::positional_mark:
        case detail::classifier_t::none:
        default:
            throw horrible_error_t("parsing got called with invalid option! You should not see this");
        }

        auto op_ptr = std::ranges::find_if(options_, [&arg_name, current_type](const option_ptr_t &opt) {
            if (current_type == detail::classifier_t::long_)
            {
                return opt->check_lname(arg_name);
            }
            if (current_type == detail::classifier_t::short_)
            {
                return opt->check_sname(arg_name);
            }
            // Only reached for detail::classifier_t::windows_style, which accepts either form.
            return opt->check_lname(arg_name) || opt->check_sname(arg_name);
        });

        // A `while` rather than an `if` purely so that the body can `break` out to the
        // code below once a match is finally found.
        while (op_ptr == std::end(options_))
        {
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

            if (allow_non_standard_options_ && current_type == detail::classifier_t::short_ && current.size() > 2)
            {
                // Retry `-abc` as though it were `--abc`, which is how a non-standard
                // multi-character short name is spelled. A failure leaves both parts
                // empty, matching the original, which discarded the return value here.
                const auto nres = detail::split_long(std::string {'-'} + current);
                const std::string narg_name = nres ? nres->name : std::string {};
                const std::string nvalue = nres ? nres->value : std::string {};

                op_ptr = std::ranges::find_if(
                    options_, [&narg_name](const option_ptr_t &opt) { return opt->check_sname(narg_name); });
                if (op_ptr != std::end(options_))
                {
                    arg_name = narg_name;
                    value = nvalue;
                    rest.clear();
                    break;
                }
            }

            // A nameless subcommand cannot fall through, so it must not swallow the
            // argument into its own missing list.
            if (parent_ != nullptr && name_.empty())
            {
                return false;
            }

            // Dotted notation, `--sub.opt=value`, is equivalent to naming the
            // subcommand and then the option.
            const auto dotloc = arg_name.find_first_of('.', 1);
            if (dotloc != std::string::npos && dotloc < arg_name.size() - 1)
            {
                auto *sub = _find_subcommand(arg_name.substr(0, dotloc), true, false);
                if (sub != nullptr)
                {
                    std::string v = args.back();
                    args.pop_back();
                    arg_name = arg_name.substr(dotloc + 1);
                    if (arg_name.size() > 1)
                    {
                        args.push_back(std::string("--") + v.substr(dotloc + 3));
                        current_type = detail::classifier_t::long_;
                    }
                    else
                    {
                        auto nval = v.substr(dotloc + 2);
                        nval.front() = '-';
                        if (nval.size() > 2)
                        {
                            // A short option cannot carry '=', so the value becomes a
                            // separate argument.
                            args.push_back(nval.substr(3));
                            nval.resize(2);
                        }
                        args.push_back(nval);
                        current_type = detail::classifier_t::short_;
                    }

                    bool val = false;
                    if ((current_type == detail::classifier_t::short_ &&
                         detail::valid_first_char(args.back()[1])) ||
                        detail::split_long(args.back()).has_value())
                    {
                        val = sub->_parse_arg(args, current_type, true);
                    }

                    if (val)
                    {
                        if (!sub->silent_)
                        {
                            parsed_subcommands_.push_back(sub);
                        }
                        increment_parsed();
                        _trigger_pre_parse(args.size());

                        // The subcommand is complete after a single dotted option, so
                        // run its completion sequence now.
                        if (sub->parse_complete_callback_)
                        {
                            sub->_process_callbacks(callback_priority_t::first_pre_help);
                            sub->_process_help_flags(callback_priority_t::first);
                            sub->_process_callbacks(callback_priority_t::first);
                            sub->_process_env();
                            sub->_process_callbacks(callback_priority_t::pre_requirements_check_pre_help);
                            sub->_process_help_flags(callback_priority_t::pre_requirements_check);
                            sub->_process_callbacks(callback_priority_t::pre_requirements_check);
                            sub->_process_requirements();
                            sub->_process_callbacks(callback_priority_t::normal_pre_help);
                            sub->_process_help_flags(callback_priority_t::normal);
                            sub->_process_callbacks(callback_priority_t::normal);
                            sub->_process_callbacks(callback_priority_t::last_pre_help);
                            sub->_process_help_flags(callback_priority_t::last);
                            sub->_process_callbacks(callback_priority_t::last);
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

            if (parent_ != nullptr && fallthrough_)
            {
                return _get_fallthrough_parent()->_parse_arg(args, current_type, false);
            }

            args.pop_back();
            _move_to_missing(current_type, current);
            if (get_prefix_command_mode() == prefix_command_mode_t::on)
            {
                while (!args.empty())
                {
                    missing_.emplace_back(detail::classifier_t::none, args.back());
                    args.pop_back();
                }
            }
            else if (allow_extras_ == extras_mode_t::assume_single_argument)
            {
                if (!args.empty() && _recognize(args.back(), false) == detail::classifier_t::none)
                {
                    _move_to_missing(detail::classifier_t::none, args.back());
                    args.pop_back();
                }
            }
            else if (allow_extras_ == extras_mode_t::assume_multiple_arguments)
            {
                while (!args.empty() && _recognize(args.back(), false) == detail::classifier_t::none)
                {
                    _move_to_missing(detail::classifier_t::none, args.back());
                    args.pop_back();
                }
            }
            return true;
        }

        args.pop_back();

        // A reference, so that the rest of the function is not shot through with
        // dereferences of the iterator.
        option_ptr_t &op = *op_ptr;

        if (op->get_inject_separator())
        {
            if (!op->results().empty() && !op->results().back().empty())
            {
                op->add_result(std::string {});
            }
        }
        if (op->get_trigger_on_parse() && op->current_option_state_ == option_t::option_state_t::callback_run)
        {
            op->clear();
        }

        const int min_num = (std::min)(op->get_type_size_min(), op->get_items_expected_min());
        int max_num = op->get_items_expected_max();

        // A container-like option with extra arguments disabled takes only one element's
        // worth per appearance. The 1/16 threshold is arbitrary; it just has to be
        // comfortably below the unbounded sentinel.
        if (max_num >= detail::expected_max_vector_size / 16 && !op->get_allow_extra_args())
        {
            auto tmax = op->get_type_size_max();
            max_num = detail::checked_multiply(tmax, op->get_expected_min()) ? tmax
                                                                            : detail::expected_max_vector_size;
        }

        int collected = 0;    // values gathered for this option overall
        int result_count = 0; // values produced by the most recent add_result

        if (max_num == 0)
        {
            // A pure flag: the value comes from the flag's own table, not the command line.
            auto res = op->get_flag_value(arg_name, value);
            op->add_result(std::move(res));
            parse_order_.push_back(op.get());
        }
        else if (!value.empty())
        {
            // --name=value
            op->add_result(value, result_count);
            parse_order_.push_back(op.get());
            collected += result_count;
        }
        else if (!rest.empty())
        {
            // -Nvalue
            op->add_result(rest, result_count);
            parse_order_.push_back(op.get());
            rest.clear();
            collected += result_count;
        }

        // Take the minimum unconditionally, even for an unbounded option.
        while (min_num > collected && !args.empty())
        {
            std::string next_arg = args.back();
            args.pop_back();
            op->add_result(next_arg, result_count);
            parse_order_.push_back(op.get());
            collected += result_count;
        }

        if (min_num > collected)
        {
            throw argument_mismatch_t::typed_at_least(op->get_name(), min_num, op->get_type_name());
        }

        if (max_num > collected || op->get_allow_extra_args())
        {
            const auto remreqpos = _count_remaining_positionals(true);
            while ((collected < max_num || op->get_allow_extra_args()) && !args.empty() &&
                   _recognize(args.back(), false) == detail::classifier_t::none)
            {
                // Leave enough arguments behind to satisfy the required positionals.
                if (remreqpos >= args.size())
                {
                    break;
                }
                if (validate_optional_arguments_)
                {
                    std::string candidate = args.back();
                    if (!op->_validate(candidate, 0).empty())
                    {
                        break;
                    }
                }
                op->add_result(args.back(), result_count);
                parse_order_.push_back(op.get());
                args.pop_back();
                collected += result_count;
            }

            // A trailing `--` closes an unbounded list, and is consumed doing so.
            if (!args.empty() && _recognize(args.back()) == detail::classifier_t::positional_mark)
            {
                args.pop_back();
            }

            // An optional flag that received nothing falls back to its default.
            if (min_num == 0 && max_num > 0 && collected == 0)
            {
                auto res = op->get_flag_value(arg_name, std::string {});
                op->add_result(std::move(res));
                parse_order_.push_back(op.get());
            }
        }

        // A partially filled compound type gets an empty placeholder when the type
        // permits a variable size, and is an error when it does not.
        if (min_num > 0 && (collected % op->get_type_size_max()) != 0)
        {
            if (op->get_type_size_max() != op->get_type_size_min())
            {
                op->add_result(std::string {});
            }
            else
            {
                throw argument_mismatch_t::partial_type(op->get_name(), op->get_type_size_min(),
                                                        op->get_type_name());
            }
        }

        if (op->get_trigger_on_parse())
        {
            op->run_callback();
        }

        // Whatever is left of a bundled short argument goes back for another pass.
        if (!rest.empty())
        {
            rest = "-" + rest;
            args.push_back(std::move(rest));
        }
        return true;
    }

    auto app_t::_trigger_pre_parse(std::size_t remaining_args) -> void
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
                // A named subcommand with an immediate callback starts fresh on each
                // appearance, but keeps its parse count and its unmatched arguments.
                const auto pcnt = parsed_;
                missing_t extras = std::move(missing_);
                clear();
                parsed_ = pcnt;
                pre_parse_called_ = true;
                missing_ = std::move(extras);
            }
        }
    }

    auto app_t::_get_fallthrough_parent() noexcept -> app_t *
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

    auto app_t::_get_fallthrough_parent() const noexcept -> const app_t *
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

    auto app_t::_compare_subcommand_names(const app_t &subcom, const app_t &base) const -> const std::string &
    {
        static const std::string empty_name;
        if (subcom.disabled_)
        {
            return empty_name;
        }

        for (const auto &subc : base.subcommands_)
        {
            if (subc.get() == &subcom)
            {
                continue;
            }
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
            // Both directions are needed: ignore_case or ignore_underscore may be set
            // on one of the two and not the other.
            for (const auto &les : subc->aliases_)
            {
                if (subcom.check_name(les))
                {
                    return les;
                }
            }

            // An option group holds subcommands of its own, so recurse into it.
            if (subc->get_name().empty())
            {
                const auto &cmpres = _compare_subcommand_names(subcom, *subc);
                if (!cmpres.empty())
                {
                    return cmpres;
                }
            }
            if (subcom.get_name().empty())
            {
                const auto &cmpres = _compare_subcommand_names(*subc, subcom);
                if (!cmpres.empty())
                {
                    return cmpres;
                }
            }
        }
        return empty_name;
    }

    /// @brief Reports whether an extras mode keeps unmatched arguments.
    ///
    /// @param mode The mode to test.
    /// @return `true` if unmatched arguments are retained rather than reported.
    auto capture_extras(extras_mode_t mode) -> bool
    {
        return mode == extras_mode_t::capture || mode == extras_mode_t::assume_single_argument ||
               mode == extras_mode_t::assume_multiple_arguments;
    }

    auto app_t::_move_to_missing(detail::classifier_t val_type, const std::string &val) -> void
    {
        if (allow_extras_ == extras_mode_t::error_immediately)
        {
            throw extras_error_t(name_, std::vector<std::string> {val});
        }

        if (capture_extras(allow_extras_) || subcommands_.empty() || get_prefix_command())
        {
            if (allow_extras_ != extras_mode_t::ignore)
            {
                missing_.emplace_back(val_type, val);
            }
            return;
        }

        // An option group willing to capture extras takes them instead.
        for (auto &subc : subcommands_)
        {
            if (subc->name_.empty() && capture_extras(subc->allow_extras_))
            {
                subc->missing_.emplace_back(val_type, val);
                return;
            }
        }

        if (allow_extras_ != extras_mode_t::ignore)
        {
            missing_.emplace_back(val_type, val);
        }
    }

    auto app_t::_move_option(option_t *opt, app_t *app) -> void
    {
        if (opt == nullptr)
        {
            throw option_not_found_t("the option is NULL");
        }

        const bool found = std::ranges::any_of(subcommands_, [app](const app_ptr_t &subc)
                                               { return app == subc.get(); });
        if (!found)
        {
            throw option_not_found_t("The Given app is not a subcommand");
        }

        if ((help_ptr_ == opt) || (help_all_ptr_ == opt))
        {
            throw option_already_added_t("cannot move help options");
        }
        if (config_ptr_ == opt)
        {
            throw option_already_added_t("cannot move config file options");
        }

        const auto iterator =
            std::ranges::find_if(options_, [opt](const option_ptr_t &v) { return v.get() == opt; });
        if (iterator == std::end(options_))
        {
            throw option_not_found_t("could not locate the given option");
        }

        const auto &opt_p = *iterator;
        const bool conflicts = std::ranges::any_of(app->options_, [&opt_p](const option_ptr_t &v)
                                                   { return (*v == *opt_p); });
        if (conflicts)
        {
            throw option_already_added_t("option was not located: " + opt->get_name());
        }

        // Erase only once the insertion has succeeded.
        app->options_.push_back(std::move(*iterator));
        options_.erase(iterator);
    }

    auto trigger_on(app_t *trigger_app, app_t *app_to_enable) -> void
    {
        app_to_enable->enabled_by_default(false);
        app_to_enable->disabled_by_default();
        trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(false); });
    }

    auto trigger_on(app_t *trigger_app, std::vector<app_t *> apps_to_enable) -> void
    {
        for (auto &app : apps_to_enable)
        {
            app->enabled_by_default(false);
            app->disabled_by_default();
        }

        trigger_app->preparse_callback([apps = std::move(apps_to_enable)](std::size_t) {
            for (auto *app : apps)
            {
                app->disabled(false);
            }
        });
    }

    auto trigger_off(app_t *trigger_app, app_t *app_to_enable) -> void
    {
        app_to_enable->disabled_by_default(false);
        app_to_enable->enabled_by_default();
        trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(); });
    }

    auto trigger_off(app_t *trigger_app, std::vector<app_t *> apps_to_enable) -> void
    {
        for (auto &app : apps_to_enable)
        {
            app->disabled_by_default(false);
            app->enabled_by_default();
        }

        trigger_app->preparse_callback([apps = std::move(apps_to_enable)](std::size_t) {
            for (auto *app : apps)
            {
                app->disabled();
            }
        });
    }

    auto deprecate_option(option_t *opt, const std::string &replacement) -> void
    {
        validator_t deprecate_warning {[opt, replacement](std::string &) {
                                           std::cout << opt->get_name() << " is deprecated please use '"
                                                     << replacement << "' instead\n";
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

    auto retire_option(app_t *app, option_t *opt) -> void
    {
        // A scratch application is used only to keep a copy of the option's shape while
        // the original is removed and replaced.
        app_t temp;
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
        validator_t retired_warning {[opt2](std::string &) {
                                         std::cout << "WARNING " << opt2->get_name()
                                                   << " is retired and has no effect\n";
                                         return std::string();
                                     },
                                     ""};
        // LCOV_EXCL_STOP
        retired_warning.application_index(0);
        opt2->check(retired_warning);
    }

    auto retire_option(app_t &app, option_t *opt) -> void
    {
        retire_option(&app, opt);
    }

    auto retire_option(app_t *app, const std::string &option_name) -> void
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
        validator_t retired_warning {[opt2](std::string &) {
                                         std::cout << "WARNING " << opt2->get_name()
                                                   << " is retired and has no effect\n";
                                         return std::string();
                                     },
                                     ""};
        // LCOV_EXCL_STOP
        retired_warning.application_index(0);
        opt2->check(retired_warning);
    }

    auto retire_option(app_t &app, const std::string &option_name) -> void
    {
        retire_option(&app, option_name);
    }

    namespace failure_message
    {

        auto simple(const app_t *app, const error_t &e) -> std::string
        {
            std::string header = std::string(e.what()) + "\n";
            std::vector<std::string> names;

            if (app->get_help_ptr() != nullptr)
            {
                names.push_back(app->get_help_ptr()->get_name());
            }
            if (app->get_help_all_ptr() != nullptr)
            {
                names.push_back(app->get_help_all_ptr()->get_name());
            }

            if (!names.empty())
            {
                header += "Run with " + detail::join(names, " or ") + " for more information.\n";
            }

            return header;
        }

        auto help(const app_t *app, const error_t &e) -> std::string
        {
            std::string header = std::string("ERROR: ") + e.get_name() + ": " + e.what() + "\n";
            header += app->help();
            return header;
        }

    } // namespace failure_message

} // namespace cli
