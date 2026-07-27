/// @file
/// @brief The option type: one command-line option and everything set on it.
///
/// An @ref cli::option_t is created by `app_t::add_option` and configured by
/// chaining: `app.add_option("--f,--file", path)->required()->check(cli::existing_file)`.
/// Settings shared with `app_t`'s option defaults live in @ref cli::option_base_t.
///
/// An option moves through @ref cli::option_t::option_state_t as parsing proceeds:
/// raw results are collected, validated, reduced according to the multi-option
/// policy, and finally handed to the callback that writes the bound variable.

module;
#include <cerrno>

export module cli11:option;

import std;
import :string_tools;
import :error;
import :split;
import :validators;
import :type_tools;

export namespace cli
{

    /// @brief The raw strings collected for one option.
    using results_t = std::vector<std::string>;

    /// @brief The callback that turns collected results into a bound value.
    using callback_t = std::function<bool(const results_t &)>;

    class option_t;
    class app_t;
    class config_base_t;

    /// @brief Owning handle to an option.
    using option_ptr_t = std::unique_ptr<option_t>;

    /// @brief Shared handle to a validator.
    using validator_ptr_t = std::shared_ptr<validator_t>;

    /// @brief What to do when an option is given more values than it expects.
    enum class multi_option_policy_t : std::uint8_t
    {
        reject,     ///< Report an error. Named `reject` because `throw` is a keyword.
        take_last,  ///< Keep only the last value.
        take_first, ///< Keep only the first value.
        join,       ///< Join every value with the option's delimiter.
        take_all,   ///< Keep every value.
        sum,        ///< Add the values together.
        reverse,    ///< Keep every value, in reverse order.
    };

    /// @brief When an option's callback runs relative to the others.
    enum class callback_priority_t : std::uint8_t
    {
        first_pre_help = 0,                  ///< Before everything, including help.
        first = 1,                           ///< Before everything else.
        pre_requirements_check_pre_help = 2, ///< Before requirement checks and help.
        pre_requirements_check = 3,          ///< Before requirement checks.
        normal_pre_help = 4,                 ///< In the usual position, but before help.
        normal = 5,                          ///< The usual position.
        last_pre_help = 6,                   ///< After everything, but before help.
        last = 7                             ///< After everything else.
    };

    /// @brief Settings shared by @ref option_t and @ref option_defaults_t.
    ///
    /// Every setter returns a pointer to the *derived* type so that chaining keeps
    /// working on the concrete class. That used to be done with CRTP; it now uses
    /// an explicit object parameter, so the class is no longer a template.
    class option_base_t
    {
            friend app_t;
            friend config_base_t;

        protected:
            /// @brief The help group this option is listed under.
            std::string group_ = std::string("OPTIONS");

            /// @brief Whether the option must appear.
            bool required_ {false};

            /// @brief Whether name matching ignores case.
            bool ignore_case_ {false};

            /// @brief Whether name matching ignores underscores.
            bool ignore_underscore_ {false};

            /// @brief Whether the option may be set from a configuration file.
            bool configurable_ {true};

            /// @brief Whether `--no-flag` style overrides are rejected.
            bool disable_flag_override_ {false};

            /// @brief Character that splits a single value into several.
            char delimiter_ {'\0'};

            /// @brief Whether the bound value's default is captured automatically.
            bool always_capture_default_ {false};

            /// @brief What to do with surplus values.
            multi_option_policy_t multi_option_policy_ {multi_option_policy_t::reject};

            /// @brief When this option's callback runs.
            callback_priority_t callback_priority_ {callback_priority_t::normal};

            /// @brief Copies every shared setting onto another option.
            ///
            /// @tparam T The destination type.
            /// @param other The option to copy onto.
            template <typename T> auto copy_to(T *other) const -> void
            {
                other->group(group_);
                other->required(required_);
                other->ignore_case(ignore_case_);
                other->ignore_underscore(ignore_underscore_);
                other->configurable(configurable_);
                other->disable_flag_override(disable_flag_override_);
                other->delimiter(delimiter_);
                other->always_capture_default(always_capture_default_);
                other->multi_option_policy(multi_option_policy_);
                other->callback_priority(callback_priority_);
            }

        public:
            /// @brief Sets the help group this option is listed under.
            ///
            /// @param self The concrete option, deduced.
            /// @param name The group name.
            /// @return A pointer to @p self, for chaining.
            /// @throws cli::incorrect_construction_t If @p name contains a newline or null.
            template <typename self_t> auto group(this self_t &self, const std::string &name) -> self_t *
            {
                if (!detail::valid_alias_name_string(name))
                {
                    throw incorrect_construction_t("Group names may not contain newlines or null characters");
                }
                self.group_ = name;
                return &self;
            }

            /// @brief Marks the option as required.
            ///
            /// @param self The concrete option, deduced.
            /// @param value Whether the option must appear.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto required(this self_t &self, bool value = true) -> self_t *
            {
                self.required_ = value;
                return &self;
            }

            /// @brief Marks the option as required.
            ///
            /// @param self The concrete option, deduced.
            /// @param value Whether the option must appear.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto mandatory(this self_t &self, bool value = true) -> self_t *
            {
                return self.required(value);
            }

            /// @brief Captures the bound variable's value as the printed default.
            ///
            /// @param self The concrete option, deduced.
            /// @param value Whether to capture automatically.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto always_capture_default(this self_t &self, bool value = true) -> self_t *
            {
                self.always_capture_default_ = value;
                return &self;
            }

            /// @brief Returns the help group this option is listed under.
            ///
            /// @return The group name.
            [[nodiscard]] auto get_group() const -> const std::string &
            {
                return group_;
            }

            /// @brief Reports whether the option must appear.
            ///
            /// @return `true` if the option is required.
            [[nodiscard]] auto get_required() const -> bool
            {
                return required_;
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

            /// @brief Reports whether the option may be set from a configuration file.
            ///
            /// @return `true` if the option is configurable.
            [[nodiscard]] auto get_configurable() const -> bool
            {
                return configurable_;
            }

            /// @brief Reports whether flag overrides are rejected.
            ///
            /// @return `true` if overrides are disabled.
            [[nodiscard]] auto get_disable_flag_override() const -> bool
            {
                return disable_flag_override_;
            }

            /// @brief Returns the character that splits a value into several.
            ///
            /// @return The delimiter, or `'\0'` if none is set.
            [[nodiscard]] auto get_delimiter() const -> char
            {
                return delimiter_;
            }

            /// @brief Reports whether defaults are captured automatically.
            ///
            /// @return `true` if defaults are captured.
            [[nodiscard]] auto get_always_capture_default() const -> bool
            {
                return always_capture_default_;
            }

            /// @brief Returns what happens to surplus values.
            ///
            /// @return The multi-option policy.
            [[nodiscard]] auto get_multi_option_policy() const -> multi_option_policy_t
            {
                return multi_option_policy_;
            }

            /// @brief Returns when this option's callback runs.
            ///
            /// @return The callback priority.
            [[nodiscard]] auto get_callback_priority() const -> callback_priority_t
            {
                return callback_priority_;
            }

            /// @brief Keeps only the last value given.
            ///
            /// @param self The concrete option, deduced.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto take_last(this self_t &self) -> self_t *
            {
                self.multi_option_policy(multi_option_policy_t::take_last);
                return &self;
            }

            /// @brief Keeps only the first value given.
            ///
            /// @param self The concrete option, deduced.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto take_first(this self_t &self) -> self_t *
            {
                self.multi_option_policy(multi_option_policy_t::take_first);
                return &self;
            }

            /// @brief Keeps every value given.
            ///
            /// @param self The concrete option, deduced.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto take_all(this self_t &self) -> self_t *
            {
                self.multi_option_policy(multi_option_policy_t::take_all);
                return &self;
            }

            /// @brief Joins every value given.
            ///
            /// @param self The concrete option, deduced.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto join(this self_t &self) -> self_t *
            {
                self.multi_option_policy(multi_option_policy_t::join);
                return &self;
            }

            /// @brief Joins every value given, using a specific delimiter.
            ///
            /// @param self The concrete option, deduced.
            /// @param delim The character placed between joined values.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto join(this self_t &self, char delim) -> self_t *
            {
                self.delimiter_ = delim;
                self.multi_option_policy(multi_option_policy_t::join);
                return &self;
            }

            /// @brief Allows or forbids setting the option from a configuration file.
            ///
            /// @param self The concrete option, deduced.
            /// @param value Whether the option is configurable.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto configurable(this self_t &self, bool value = true) -> self_t *
            {
                self.configurable_ = value;
                return &self;
            }

            /// @brief Sets the character that splits a value into several.
            ///
            /// @param self The concrete option, deduced.
            /// @param value The delimiter, or `'\0'` for none.
            /// @return A pointer to @p self, for chaining.
            template <typename self_t> auto delimiter(this self_t &self, char value = '\0') -> self_t *
            {
                self.delimiter_ = value;
                return &self;
            }
    };

    /// @brief The settings newly created options inherit.
    ///
    /// Reachable through `app_t::option_defaults()`; changing a value here affects
    /// options added afterwards, not ones already created.
    class option_defaults_t : public option_base_t
    {
        public:
            option_defaults_t() = default;

            /// @brief Sets the default callback priority.
            ///
            /// @param value When callbacks run.
            /// @return A pointer to this object, for chaining.
            auto callback_priority(callback_priority_t value = callback_priority_t::normal) -> option_defaults_t *
            {
                callback_priority_ = value;
                return this;
            }

            /// @brief Sets the default multi-option policy.
            ///
            /// @param value What to do with surplus values.
            /// @return A pointer to this object, for chaining.
            auto multi_option_policy(multi_option_policy_t value = multi_option_policy_t::reject) -> option_defaults_t *
            {
                multi_option_policy_ = value;
                return this;
            }

            /// @brief Sets whether name matching ignores case by default.
            ///
            /// @param value Whether to ignore case.
            /// @return A pointer to this object, for chaining.
            auto ignore_case(bool value = true) -> option_defaults_t *
            {
                ignore_case_ = value;
                return this;
            }

            /// @brief Sets whether name matching ignores underscores by default.
            ///
            /// @param value Whether to ignore underscores.
            /// @return A pointer to this object, for chaining.
            auto ignore_underscore(bool value = true) -> option_defaults_t *
            {
                ignore_underscore_ = value;
                return this;
            }

            /// @brief Sets whether flag overrides are rejected by default.
            ///
            /// @param value Whether to disable overrides.
            /// @return A pointer to this object, for chaining.
            auto disable_flag_override(bool value = true) -> option_defaults_t *
            {
                disable_flag_override_ = value;
                return this;
            }

            /// @brief Sets the default value delimiter.
            ///
            /// @param value The delimiter, or `'\0'` for none.
            /// @return A pointer to this object, for chaining.
            auto delimiter(char value = '\0') -> option_defaults_t *
            {
                delimiter_ = value;
                return this;
            }
    };

    /// @brief One command-line option, flag, or positional argument.
    ///
    /// Not constructed directly; `app_t::add_option`, `add_flag`, and their
    /// relatives create these and hand back a pointer for chaining.
    class option_t : public option_base_t
    {
            friend app_t;
            friend config_base_t;

        protected:
            /// @brief Short names, without their leading dash.
            std::vector<std::string> snames_ {};

            /// @brief Long names, without their leading dashes.
            std::vector<std::string> lnames_ {};

            /// @brief Flag names paired with the default value each implies.
            std::vector<std::pair<std::string, std::string>> default_flag_values_ {};

            /// @brief Every flag name, in the order given.
            std::vector<std::string> fnames_ {};

            /// @brief The positional name, empty if this is not a positional.
            std::string pname_ {};

            /// @brief The environment variable consulted when the option is absent.
            std::string envname_ {};

            /// @brief The description shown in help output.
            std::string description_ {};

            /// @brief The default value as shown in help output.
            std::string default_str_ {};

            /// @brief Text replacing the generated type name in help output.
            std::string option_text_ {};

            /// @brief Produces the type name shown in help output.
            std::function<std::string()> type_name_ {[] { return std::string(); }};

            /// @brief Produces the default value shown in help output.
            std::function<std::string()> default_function_ {};

            /// @brief Largest number of values one appearance consumes.
            int type_size_max_ {1};

            /// @brief Smallest number of values one appearance consumes.
            int type_size_min_ {1};

            /// @brief Smallest number of values accepted in total.
            int expected_min_ {1};

            /// @brief Largest number of values accepted in total.
            int expected_max_ {1};

            /// @brief Validators and transforms, applied in order.
            std::vector<validator_ptr_t> validators_ {};

            /// @brief Options that must also appear.
            std::set<option_t *> needs_ {};

            /// @brief Options that must not appear alongside this one.
            std::set<option_t *> excludes_ {};

            /// @brief The application this option belongs to.
            app_t *parent_ {nullptr};

            /// @brief Writes the collected results into the bound variable.
            callback_t callback_ {};

            /// @brief The raw strings collected during parsing.
            results_t results_ {};

            /// @brief The results after validation and reduction.
            mutable results_t proc_results_ {};

            /// @brief How far an option has progressed through parsing.
            enum class option_state_t : std::uint8_t
            {
                parsing = 0,     ///< Collecting raw results.
                validated = 2,   ///< Validators have run.
                reduced = 4,     ///< The multi-option policy has been applied.
                callback_run = 6 ///< The callback has written the bound variable.
            };

            /// @brief How far this option has progressed.
            option_state_t current_option_state_ {option_state_t::parsing};

            /// @brief Whether surplus arguments are absorbed by this option.
            bool allow_extra_args_ {false};

            /// @brief Whether this option behaves like a flag.
            bool flag_like_ {false};

            /// @brief Whether the callback runs even when only the default is present.
            bool run_callback_for_default_ {false};

            /// @brief Whether a separator is inserted between value groups.
            bool inject_separator_ {false};

            /// @brief Whether the callback runs as soon as a value is parsed.
            bool trigger_on_result_ {false};

            /// @brief Whether the callback runs even when the option is absent.
            bool force_callback_ {false};

            /// @brief Constructs an option from a name specification.
            ///
            /// @param option_name The name specification, for example `"-f,--file"`.
            /// @param option_description The description shown in help output.
            /// @param callback Writes collected results into the bound variable.
            /// @param parent The owning application.
            /// @param allow_non_standard Accept multi-character short names.
            /// @throws cli::bad_name_string_t If the specification is malformed.
            option_t(std::string option_name,
                     std::string option_description,
                     callback_t callback,
                     app_t *parent,
                     bool allow_non_standard = false)
                : description_(std::move(option_description)), parent_(parent), callback_(std::move(callback))
            {
                auto names = detail::get_names(detail::split_names(option_name), allow_non_standard);
                snames_ = std::move(names.short_names);
                lnames_ = std::move(names.long_names);
                pname_ = std::move(names.positional_name);
            }

        public:
            option_t(const option_t &) = delete;
            auto operator=(const option_t &) -> option_t & = delete;

            /// @brief Returns how many values were collected.
            ///
            /// @return The result count.
            [[nodiscard]] auto count() const -> std::size_t
            {
                return results_.size();
            }

            /// @brief Reports whether no values were collected.
            ///
            /// @return `true` if nothing was collected.
            [[nodiscard]] auto empty() const -> bool
            {
                return results_.empty();
            }

            /// @brief Reports whether the option was used.
            ///
            /// @return `true` if values were collected or the callback is forced.
            explicit operator bool() const
            {
                return !empty() || force_callback_;
            }

            /// @brief Discards collected values and resets the parse state.
            auto clear() -> void
            {
                results_.clear();
                current_option_state_ = option_state_t::parsing;
            }

            /// @name Setting options
            ///@{

            /// @brief Sets how many values the option accepts.
            ///
            /// A negative value sets a minimum with no maximum. Zero makes the option
            /// behave as a flag.
            ///
            /// @param value The value count.
            /// @return A pointer to this option, for chaining.
            auto expected(int value) -> option_t *
            {
                if (value < 0)
                {
                    expected_min_ = -value;
                    if (expected_max_ < expected_min_)
                    {
                        expected_max_ = expected_min_;
                    }
                    allow_extra_args_ = true;
                    flag_like_ = false;
                }
                else if (value == detail::expected_max_vector_size)
                {
                    expected_min_ = 1;
                    expected_max_ = detail::expected_max_vector_size;
                    allow_extra_args_ = true;
                    flag_like_ = false;
                }
                else
                {
                    expected_min_ = value;
                    expected_max_ = value;
                    flag_like_ = (expected_min_ == 0);
                }
                return this;
            }

            /// @brief Sets the range of value counts the option accepts.
            ///
            /// Negative bounds are treated as their magnitude, except that a negative
            /// maximum means unbounded. The bounds are swapped if given in the wrong order.
            ///
            /// @param value_min The smallest acceptable count.
            /// @param value_max The largest acceptable count.
            /// @return A pointer to this option, for chaining.
            auto expected(int value_min, int value_max) -> option_t *
            {
                if (value_min < 0)
                {
                    value_min = -value_min;
                }
                if (value_max < 0)
                {
                    value_max = detail::expected_max_vector_size;
                }
                if (value_max < value_min)
                {
                    expected_min_ = value_max;
                    expected_max_ = value_min;
                }
                else
                {
                    expected_max_ = value_max;
                    expected_min_ = value_min;
                }
                return this;
            }

            /// @brief Allows the option to absorb surplus arguments.
            ///
            /// @param value Whether to absorb surplus arguments.
            /// @return A pointer to this option, for chaining.
            auto allow_extra_args(bool value = true) -> option_t *
            {
                allow_extra_args_ = value;
                return this;
            }

            /// @brief Reports whether the option absorbs surplus arguments.
            ///
            /// @return `true` if surplus arguments are absorbed.
            [[nodiscard]] auto get_allow_extra_args() const -> bool
            {
                return allow_extra_args_;
            }

            /// @brief Runs the callback as soon as a value is parsed.
            ///
            /// @param value Whether to trigger during parsing.
            /// @return A pointer to this option, for chaining.
            auto trigger_on_parse(bool value = true) -> option_t *
            {
                trigger_on_result_ = value;
                return this;
            }

            /// @brief Reports whether the callback runs during parsing.
            ///
            /// @return `true` if the callback is triggered during parsing.
            [[nodiscard]] auto get_trigger_on_parse() const -> bool
            {
                return trigger_on_result_;
            }

            /// @brief Runs the callback even when the option is absent.
            ///
            /// @param value Whether to force the callback.
            /// @return A pointer to this option, for chaining.
            auto force_callback(bool value = true) -> option_t *
            {
                force_callback_ = value;
                return this;
            }

            /// @brief Reports whether the callback runs when the option is absent.
            ///
            /// @return `true` if the callback is forced.
            [[nodiscard]] auto get_force_callback() const -> bool
            {
                return force_callback_;
            }

            /// @brief Runs the callback when only the default value is present.
            ///
            /// @param value Whether to run the callback for defaults.
            /// @return A pointer to this option, for chaining.
            auto run_callback_for_default(bool value = true) -> option_t *
            {
                run_callback_for_default_ = value;
                return this;
            }

            /// @brief Reports whether the callback runs for default values.
            ///
            /// @return `true` if the callback runs for defaults.
            [[nodiscard]] auto get_run_callback_for_default() const -> bool
            {
                return run_callback_for_default_;
            }

            /// @brief Sets when this option's callback runs.
            ///
            /// @param value The callback priority.
            /// @return A pointer to this option, for chaining.
            auto callback_priority(callback_priority_t value = callback_priority_t::normal) -> option_t *
            {
                callback_priority_ = value;
                return this;
            }

            ///@}
            /// @name Validators and transforms
            ///
            /// A check inspects a value; a transform rewrites it. Checks are appended
            /// and run after any transforms, which are prepended.
            ///@{

            /// @brief Appends a shared validator as a check.
            ///
            /// @param validator The validator to append.
            /// @return A pointer to this option, for chaining.
            auto check(validator_ptr_t validator) -> option_t *
            {
                validator->non_modifying();
                validators_.push_back(std::move(validator));
                return this;
            }

            /// @brief Appends a validator as a check.
            ///
            /// @param validator The validator to append.
            /// @param validator_name A name to find it by later.
            /// @return A pointer to this option, for chaining.
            auto check(validator_t validator, const std::string &validator_name = "") -> option_t *
            {
                validator.non_modifying();
                auto vp = std::make_shared<validator_t>(std::move(validator));
                if (!validator_name.empty())
                {
                    vp->name(validator_name);
                }
                validators_.push_back(std::move(vp));
                return this;
            }

            /// @brief Appends a callable as a check.
            ///
            /// @param validator_func Returns an empty string on success, else a message.
            /// @param validator_description The description shown in help output.
            /// @param validator_name A name to find it by later.
            /// @return A pointer to this option, for chaining.
            auto check(std::function<std::string(const std::string &)> validator_func,
                       std::string validator_description = "",
                       std::string validator_name = "") -> option_t *
            {
                auto vp = std::make_shared<validator_t>(
                    [func = std::move(validator_func)](std::string &val) { return func(val); },
                    std::move(validator_description),
                    std::move(validator_name));
                vp->non_modifying();
                validators_.push_back(std::move(vp));
                return this;
            }

            /// @brief Prepends a shared validator as a transform.
            ///
            /// @param validator The validator to prepend.
            /// @return A pointer to this option, for chaining.
            auto transform(validator_ptr_t validator) -> option_t *
            {
                validators_.insert(validators_.begin(), std::move(validator));
                return this;
            }

            /// @brief Prepends a validator as a transform.
            ///
            /// @param validator The validator to prepend.
            /// @param transform_name A name to find it by later.
            /// @return A pointer to this option, for chaining.
            auto transform(validator_t validator, const std::string &transform_name = "") -> option_t *
            {
                auto vp = std::make_shared<validator_t>(std::move(validator));
                if (!transform_name.empty())
                {
                    vp->name(transform_name);
                }
                validators_.insert(validators_.begin(), std::move(vp));
                return this;
            }

            /// @brief Prepends a callable as a transform.
            ///
            /// @param transform_func Returns the rewritten value.
            /// @param transform_description The description shown in help output.
            /// @param transform_name A name to find it by later.
            /// @return A pointer to this option, for chaining.
            auto transform(std::function<std::string(std::string)> transform_func,
                           std::string transform_description = "",
                           std::string transform_name = "") -> option_t *
            {
                auto vp = std::make_shared<validator_t>(
                    [func = std::move(transform_func)](std::string &val) {
                        val = func(val);
                        return std::string {};
                    },
                    std::move(transform_description),
                    std::move(transform_name));
                validators_.insert(validators_.begin(), std::move(vp));
                return this;
            }

            /// @brief Appends a callable run once per value, for its side effects.
            ///
            /// @param func The callable to run.
            /// @return A pointer to this option, for chaining.
            auto each(std::function<void(std::string)> func) -> option_t *
            {
                auto vp = std::make_shared<validator_t>(
                    [f = std::move(func)](std::string &inout) {
                        f(inout);
                        return std::string {};
                    },
                    std::string {});
                validators_.push_back(std::move(vp));
                return this;
            }

            /// @brief Finds a validator by name.
            ///
            /// An empty name returns the first validator.
            ///
            /// @param validator_name The name to look for.
            /// @return A pointer to the validator.
            /// @throws cli::option_not_found_t If no validator matches.
            auto get_validator(const std::string &validator_name = "") -> validator_t *
            {
                for (auto &validator : validators_)
                {
                    if (validator_name == validator->get_name())
                    {
                        return validator.get();
                    }
                }
                if ((validator_name.empty()) && (!validators_.empty()))
                {
                    return validators_.front().get();
                }
                throw option_not_found_t(std::string {"Validator "} + validator_name + " Not Found");
            }

            /// @brief Finds a validator by position.
            ///
            /// @param index The position to look up.
            /// @return A pointer to the validator.
            /// @throws cli::option_not_found_t If @p index is out of range.
            auto get_validator(int index) -> validator_t *
            {
                if (index >= 0 && index < static_cast<int>(validators_.size()))
                {
                    return validators_[static_cast<decltype(validators_)::size_type>(index)].get();
                }
                throw option_not_found_t("Validator index is not valid");
            }

            ///@}
            /// @name Dependencies
            ///@{

            /// @brief Requires another option to appear alongside this one.
            ///
            /// Self-dependency is ignored rather than reported.
            ///
            /// @param opt The option that must also appear.
            /// @return A pointer to this option, for chaining.
            auto needs(option_t *opt) -> option_t *
            {
                if (opt != this)
                {
                    needs_.insert(opt);
                }
                return this;
            }

            /// @brief Requires a named option to appear alongside this one.
            ///
            /// @tparam T The application type to look the name up on.
            /// @param opt_name The name of the required option.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::incorrect_construction_t If no such option exists.
            template <typename T = app_t> auto needs(std::string opt_name) -> option_t *
            {
                auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
                if (opt == nullptr)
                {
                    throw incorrect_construction_t::missing_option(opt_name);
                }
                return needs(opt);
            }

            /// @brief Requires several options to appear alongside this one.
            ///
            /// @param opt The first required option.
            /// @param opt1 The second required option.
            /// @param args Any further required options.
            /// @return A pointer to this option, for chaining.
            template <typename A, typename B, typename... args_t>
            auto needs(A opt, B opt1, args_t... args) -> option_t *
            {
                needs(opt);
                return needs(opt1, args...);
            }

            /// @brief Drops a dependency.
            ///
            /// @param opt The option to stop requiring.
            /// @return `true` if the dependency was present.
            auto remove_needs(option_t *opt) -> bool
            {
                return needs_.erase(opt) > 0;
            }

            /// @brief Forbids another option from appearing alongside this one.
            ///
            /// The exclusion is recorded on both options.
            ///
            /// @param opt The option to exclude.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::incorrect_construction_t If @p opt is this option.
            auto excludes(option_t *opt) -> option_t *
            {
                if (opt == this)
                {
                    throw incorrect_construction_t("and option cannot exclude itself");
                }
                excludes_.insert(opt);
                opt->excludes_.insert(this);
                return this;
            }

            /// @brief Forbids a named option from appearing alongside this one.
            ///
            /// @tparam T The application type to look the name up on.
            /// @param opt_name The name of the excluded option.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::incorrect_construction_t If no such option exists.
            template <typename T = app_t> auto excludes(std::string opt_name) -> option_t *
            {
                auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
                if (opt == nullptr)
                {
                    throw incorrect_construction_t::missing_option(opt_name);
                }
                return excludes(opt);
            }

            /// @brief Forbids several options from appearing alongside this one.
            ///
            /// @param opt The first excluded option.
            /// @param opt1 The second excluded option.
            /// @param args Any further excluded options.
            /// @return A pointer to this option, for chaining.
            template <typename A, typename B, typename... args_t>
            auto excludes(A opt, B opt1, args_t... args) -> option_t *
            {
                excludes(opt);
                return excludes(opt1, args...);
            }

            /// @brief Drops an exclusion.
            ///
            /// @param opt The option to stop excluding.
            /// @return `true` if the exclusion was present.
            auto remove_excludes(option_t *opt) -> bool
            {
                return excludes_.erase(opt) > 0;
            }

            /// @brief Sets the environment variable consulted when the option is absent.
            ///
            /// @param name The variable name.
            /// @return A pointer to this option, for chaining.
            auto envname(std::string name) -> option_t *
            {
                envname_ = std::move(name);
                return this;
            }

            ///@}

            /// @brief Makes name matching case-insensitive.
            ///
            /// Enabling this can make two previously distinct options collide, so the
            /// sibling options are rescanned and the change is rolled back on conflict.
            ///
            /// @tparam T The application type holding the sibling options.
            /// @param value Whether to ignore case.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::option_already_added_t If the change causes a name conflict.
            template <typename T = app_t> auto ignore_case(bool value = true) -> option_t *
            {
                if (!ignore_case_ && value)
                {
                    ignore_case_ = true;
                    const auto conflict = _find_name_conflict<T>();
                    if (!conflict.empty())
                    {
                        ignore_case_ = false;
                        throw option_already_added_t("adding ignore case caused a name conflict with " + conflict);
                    }
                }
                else
                {
                    ignore_case_ = value;
                }
                return this;
            }

            /// @brief Makes name matching ignore underscores.
            ///
            /// Enabling this can make two previously distinct options collide, so the
            /// sibling options are rescanned and the change is rolled back on conflict.
            ///
            /// @tparam T The application type holding the sibling options.
            /// @param value Whether to ignore underscores.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::option_already_added_t If the change causes a name conflict.
            template <typename T = app_t> auto ignore_underscore(bool value = true) -> option_t *
            {
                if (!ignore_underscore_ && value)
                {
                    ignore_underscore_ = true;
                    const auto conflict = _find_name_conflict<T>();
                    if (!conflict.empty())
                    {
                        ignore_underscore_ = false;
                        throw option_already_added_t("adding ignore underscore caused a name conflict with " +
                                                     conflict);
                    }
                }
                else
                {
                    ignore_underscore_ = value;
                }
                return this;
            }

            /// @brief Sets what happens to surplus values.
            ///
            /// Moving away from @ref multi_option_policy_t::reject on an unbounded
            /// option pins the maximum to the minimum, and resets the parse state so
            /// that already-collected results are reduced under the new policy.
            ///
            /// @param value The policy to apply.
            /// @return A pointer to this option, for chaining.
            auto multi_option_policy(multi_option_policy_t value = multi_option_policy_t::reject) -> option_t *
            {
                if (value != multi_option_policy_)
                {
                    if (multi_option_policy_ == multi_option_policy_t::reject &&
                        expected_max_ == detail::expected_max_vector_size && expected_min_ > 1)
                    {
                        expected_max_ = expected_min_;
                    }
                    multi_option_policy_ = value;
                    current_option_state_ = option_state_t::parsing;
                }
                return this;
            }

            /// @brief Rejects `--no-flag` style overrides.
            ///
            /// @param value Whether to reject overrides.
            /// @return A pointer to this option, for chaining.
            auto disable_flag_override(bool value = true) -> option_t *
            {
                disable_flag_override_ = value;
                return this;
            }

            ///@}
            /// @name Accessors
            ///@{

            /// @brief Returns the smallest number of values one appearance consumes.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_type_size() const -> int
            {
                return type_size_min_;
            }

            /// @brief Returns the smallest number of values one appearance consumes.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_type_size_min() const -> int
            {
                return type_size_min_;
            }

            /// @brief Returns the largest number of values one appearance consumes.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_type_size_max() const -> int
            {
                return type_size_max_;
            }

            /// @brief Reports whether a separator is inserted between value groups.
            ///
            /// @return `true` if a separator is inserted.
            [[nodiscard]] auto get_inject_separator() const -> bool
            {
                return inject_separator_;
            }

            /// @brief Returns the environment variable consulted when the option is absent.
            ///
            /// @return The variable name, empty if none is set.
            [[nodiscard]] auto get_envname() const -> const std::string &
            {
                return envname_;
            }

            /// @brief Returns the options this one depends on.
            ///
            /// @return The dependency set.
            [[nodiscard]] auto get_needs() const -> const std::set<option_t *> &
            {
                return needs_;
            }

            /// @brief Returns the options this one excludes.
            ///
            /// @return The exclusion set.
            [[nodiscard]] auto get_excludes() const -> const std::set<option_t *> &
            {
                return excludes_;
            }

            /// @brief Returns the default value as shown in help output.
            ///
            /// @return The rendered default.
            [[nodiscard]] auto get_default_str() const -> const std::string &
            {
                return default_str_;
            }

            /// @brief Returns the callback that writes the bound variable.
            ///
            /// @return The callback.
            [[nodiscard]] auto get_callback() const -> const callback_t &
            {
                return callback_;
            }

            /// @brief Returns the long names, without their leading dashes.
            ///
            /// @return The long names.
            [[nodiscard]] auto get_lnames() const -> const std::vector<std::string> &
            {
                return lnames_;
            }

            /// @brief Returns the short names, without their leading dash.
            ///
            /// @return The short names.
            [[nodiscard]] auto get_snames() const -> const std::vector<std::string> &
            {
                return snames_;
            }

            /// @brief Returns the flag names.
            ///
            /// @return The flag names.
            [[nodiscard]] auto get_fnames() const -> const std::vector<std::string> &
            {
                return fnames_;
            }

            /// @brief Returns one representative name for this option.
            ///
            /// Prefers the first long name, then the first short name, then the
            /// positional name, then the environment variable name.
            ///
            /// @return The chosen name.
            [[nodiscard]] auto get_single_name() const -> const std::string &
            {
                if (!lnames_.empty())
                {
                    return lnames_[0];
                }
                if (!snames_.empty())
                {
                    return snames_[0];
                }
                if (!pname_.empty())
                {
                    return pname_;
                }
                return envname_;
            }

            /// @brief Returns the smallest number of values accepted in total.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_expected() const -> int
            {
                return expected_min_;
            }

            /// @brief Returns the smallest number of values accepted in total.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_expected_min() const -> int
            {
                return expected_min_;
            }

            /// @brief Returns the largest number of values accepted in total.
            ///
            /// @return The value count.
            [[nodiscard]] auto get_expected_max() const -> int
            {
                return expected_max_;
            }

            /// @brief Returns the smallest total number of items accepted.
            ///
            /// @return The item count.
            [[nodiscard]] auto get_items_expected_min() const -> int
            {
                return type_size_min_ * expected_min_;
            }

            /// @brief Returns the largest total number of items accepted.
            ///
            /// Saturates rather than overflowing: if the product does not fit, the
            /// unbounded sentinel is reported instead.
            ///
            /// @return The item count.
            [[nodiscard]] auto get_items_expected_max() const -> int
            {
                int t = type_size_max_;
                return detail::checked_multiply(t, expected_max_) ? t : detail::expected_max_vector_size;
            }

            /// @brief Returns the smallest total number of items accepted.
            ///
            /// @return The item count.
            [[nodiscard]] auto get_items_expected() const -> int
            {
                return get_items_expected_min();
            }

            /// @brief Reports whether this option is a positional.
            ///
            /// @return `true` if a positional name is set.
            [[nodiscard]] auto get_positional() const -> bool
            {
                return !pname_.empty();
            }

            /// @brief Reports whether this option has a dashed name.
            ///
            /// @return `true` if a short or long name is set.
            [[nodiscard]] auto nonpositional() const -> bool
            {
                return (!lnames_.empty() || !snames_.empty());
            }

            /// @brief Reports whether a description was set.
            ///
            /// @return `true` if the description is not empty.
            [[nodiscard]] auto has_description() const -> bool
            {
                return !description_.empty();
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
            /// @param option_description The new description.
            /// @return A pointer to this option, for chaining.
            auto description(std::string option_description) -> option_t *
            {
                description_ = std::move(option_description);
                return this;
            }

            /// @brief Replaces the generated type name in help output.
            ///
            /// @param text The replacement text.
            /// @return A pointer to this option, for chaining.
            auto option_text(std::string text) -> option_t *
            {
                option_text_ = std::move(text);
                return this;
            }

            /// @brief Returns the text replacing the generated type name.
            ///
            /// @return The replacement text, empty if none is set.
            [[nodiscard]] auto get_option_text() const -> const std::string &
            {
                return option_text_;
            }

            ///@}
            /// @name Help output
            ///@{

            /// @brief Renders this option's name for help output.
            ///
            /// Returns an empty string when the option has no group, since a
            /// group-less option is hidden.
            ///
            /// @param positional Prefer the positional name.
            /// @param all_options List every name rather than one representative.
            /// @param disable_default_flag_values Omit the `{value}` suffix on flags.
            /// @return The rendered name.
            [[nodiscard]] auto get_name(bool positional = false,
                                        bool all_options = false,
                                        bool disable_default_flag_values = false) const -> std::string
            {
                if (get_group().empty())
                {
                    return {};
                }

                if (all_options)
                {
                    std::vector<std::string> name_list;
                    if ((positional && (!pname_.empty())) || (snames_.empty() && lnames_.empty()))
                    {
                        name_list.push_back(pname_);
                    }
                    if ((get_items_expected() == 0) && (!fnames_.empty()))
                    {
                        for (const std::string &sname : snames_)
                        {
                            name_list.push_back("-" + sname);
                            if (!disable_default_flag_values && check_fname(sname))
                            {
                                name_list.back() += "{" + get_flag_value(sname, "") + "}";
                            }
                        }
                        for (const std::string &lname : lnames_)
                        {
                            name_list.push_back("--" + lname);
                            if (!disable_default_flag_values && check_fname(lname))
                            {
                                name_list.back() += "{" + get_flag_value(lname, "") + "}";
                            }
                        }
                    }
                    else
                    {
                        for (const std::string &sname : snames_)
                        {
                            name_list.push_back("-" + sname);
                        }
                        for (const std::string &lname : lnames_)
                        {
                            name_list.push_back("--" + lname);
                        }
                    }
                    return detail::join(name_list);
                }

                if (positional)
                {
                    return pname_;
                }
                if (!lnames_.empty())
                {
                    return std::string(2, '-') + lnames_[0];
                }
                if (!snames_.empty())
                {
                    return std::string(1, '-') + snames_[0];
                }
                return pname_;
            }

            ///@}
            /// @name Parsing
            ///@{

            /// @brief Validates, reduces, and hands the results to the callback.
            ///
            /// Advances the option through @ref option_state_t, skipping stages that
            /// have already run. When the callback is forced and nothing was collected,
            /// the printed default is used and then discarded again.
            ///
            /// @throws cli::conversion_error_t If the callback rejects the results.
            auto run_callback() -> void
            {
                bool used_default_str = false;
                if (force_callback_ && results_.empty())
                {
                    used_default_str = true;
                    add_result(default_str_);
                }
                if (current_option_state_ == option_state_t::parsing)
                {
                    _validate_results(results_);
                    current_option_state_ = option_state_t::validated;
                }
                if (current_option_state_ < option_state_t::reduced)
                {
                    _reduce_results(proc_results_, results_);
                }
                current_option_state_ = option_state_t::callback_run;
                if (callback_)
                {
                    const results_t &send_results = proc_results_.empty() ? results_ : proc_results_;
                    if (send_results.empty())
                    {
                        return;
                    }
                    const bool local_result = callback_(send_results);
                    if (used_default_str)
                    {
                        results_.clear();
                        proc_results_.clear();
                    }
                    if (!local_result)
                    {
                        throw conversion_error_t(get_name(), results_);
                    }
                }
            }

            /// @brief Returns the first name this option shares with another.
            ///
            /// Two options conflict if any of their names collide. When both are
            /// configurable the comparison is looser, because a configuration file
            /// does not distinguish short from long names.
            ///
            /// @param other The option to compare against.
            /// @return The colliding name, or an empty string if there is none.
            [[nodiscard]] auto matching_name(const option_t &other) const -> const std::string &
            {
                static const std::string empty_name;
                const bool both_configurable = configurable_ && other.configurable_;

                for (const std::string &sname : snames_)
                {
                    if (other.check_sname(sname))
                    {
                        return sname;
                    }
                    if (both_configurable && other.check_lname(sname))
                    {
                        return sname;
                    }
                }
                for (const std::string &lname : lnames_)
                {
                    if (other.check_lname(lname))
                    {
                        return lname;
                    }
                    if (lname.size() == 1 && both_configurable)
                    {
                        if (other.check_sname(lname))
                        {
                            return lname;
                        }
                    }
                }
                if (both_configurable && snames_.empty() && lnames_.empty() && !pname_.empty())
                {
                    if (other.check_sname(pname_) || other.check_lname(pname_) || pname_ == other.pname_)
                    {
                        return pname_;
                    }
                }
                if (both_configurable && other.snames_.empty() && other.fnames_.empty() && !other.pname_.empty())
                {
                    if (check_sname(other.pname_) || check_lname(other.pname_) || (pname_ == other.pname_))
                    {
                        return other.pname_;
                    }
                }
                if (ignore_case_ || ignore_underscore_)
                {
                    for (const std::string &sname : other.snames_)
                    {
                        if (check_sname(sname))
                        {
                            return sname;
                        }
                    }
                    for (const std::string &lname : other.lnames_)
                    {
                        if (check_lname(lname))
                        {
                            return lname;
                        }
                    }
                }
                return empty_name;
            }

            /// @brief Reports whether two options share a name.
            ///
            /// @param other The option to compare against.
            /// @return `true` if any name collides.
            [[nodiscard]] auto operator==(const option_t &other) const -> bool
            {
                return !matching_name(other).empty();
            }

            /// @brief Reports whether a command-line token names this option.
            ///
            /// Accepts `--long`, `-s`, a bare positional name, or the environment
            /// variable name.
            ///
            /// @param name The token to test.
            /// @return `true` if the token names this option.
            [[nodiscard]] auto check_name(const std::string &name) const -> bool
            {
                if (name.length() > 2 && name[0] == '-' && name[1] == '-')
                {
                    return check_lname(name.substr(2));
                }
                if (name.length() > 1 && name.front() == '-')
                {
                    return check_sname(name.substr(1));
                }
                if (!pname_.empty())
                {
                    std::string local_pname = pname_;
                    std::string local_name = name;
                    if (ignore_underscore_)
                    {
                        local_pname = detail::remove_underscore(local_pname);
                        local_name = detail::remove_underscore(local_name);
                    }
                    if (ignore_case_)
                    {
                        local_pname = detail::to_lower(local_pname);
                        local_name = detail::to_lower(local_name);
                    }
                    if (local_name == local_pname)
                    {
                        return true;
                    }
                }
                if (!envname_.empty())
                {
                    return (name == envname_);
                }
                return false;
            }

            /// @brief Reports whether a short name belongs to this option.
            ///
            /// @param name The name to test, without its dash.
            /// @return `true` if the name matches.
            [[nodiscard]] auto check_sname(std::string name) const -> bool
            {
                return detail::find_member(std::move(name), snames_, ignore_case_).has_value();
            }

            /// @brief Reports whether a long name belongs to this option.
            ///
            /// @param name The name to test, without its dashes.
            /// @return `true` if the name matches.
            [[nodiscard]] auto check_lname(std::string name) const -> bool
            {
                return detail::find_member(std::move(name), lnames_, ignore_case_, ignore_underscore_).has_value();
            }

            /// @brief Reports whether a flag name belongs to this option.
            ///
            /// @param name The name to test.
            /// @return `true` if the name matches.
            [[nodiscard]] auto check_fname(std::string name) const -> bool
            {
                if (fnames_.empty())
                {
                    return false;
                }
                return detail::find_member(std::move(name), fnames_, ignore_case_, ignore_underscore_).has_value();
            }

            /// @brief Resolves the value a flag contributes.
            ///
            /// With no explicit value the flag's registered default is used. A negated
            /// flag inverts whatever was supplied. When overrides are disabled, any
            /// value other than the registered one is rejected.
            ///
            /// @param name The flag name as written on the command line.
            /// @param input_value The value supplied with it, if any.
            /// @return The resolved value.
            /// @throws cli::argument_mismatch_t If an override is supplied but disabled.
            [[nodiscard]] auto get_flag_value(const std::string &name, std::string input_value) const -> std::string
            {
                static constexpr std::string_view true_string {"true"};
                static constexpr std::string_view false_string {"false"};
                static constexpr std::string_view empty_string {"{}"};

                if (disable_flag_override_)
                {
                    if (!((input_value.empty()) || (input_value == empty_string)))
                    {
                        const auto default_ind = detail::find_member(name, fnames_, ignore_case_, ignore_underscore_);
                        if (default_ind.has_value())
                        {
                            if (default_flag_values_[*default_ind].second != input_value)
                            {
                                if (input_value == default_str_ && force_callback_)
                                {
                                    return input_value;
                                }
                                throw argument_mismatch_t::flag_override(name);
                            }
                        }
                        else
                        {
                            if (input_value != true_string)
                            {
                                throw argument_mismatch_t::flag_override(name);
                            }
                        }
                    }
                }

                const auto ind = detail::find_member(name, fnames_, ignore_case_, ignore_underscore_);
                if ((input_value.empty()) || (input_value == empty_string))
                {
                    if (flag_like_)
                    {
                        return ind.has_value() ? default_flag_values_[*ind].second : std::string {true_string};
                    }
                    return ind.has_value() ? default_flag_values_[*ind].second : default_str_;
                }
                if (!ind.has_value())
                {
                    return input_value;
                }
                if (default_flag_values_[*ind].second == false_string)
                {
                    errno = 0;
                    const auto val = detail::to_flag_value(input_value);
                    if (errno != 0)
                    {
                        errno = 0;
                        return input_value;
                    }
                    if (val == 1)
                    {
                        return std::string {false_string};
                    }
                    if (val == -1)
                    {
                        return std::string {true_string};
                    }
                    return std::to_string(-val);
                }
                return input_value;
            }

            /// @brief Adds one raw result, resetting the parse state.
            ///
            /// @param s The result to add.
            /// @return A pointer to this option, for chaining.
            auto add_result(std::string s) -> option_t *
            {
                _add_result(std::move(s), results_);
                current_option_state_ = option_state_t::parsing;
                return this;
            }

            /// @brief Adds one raw result and reports how many entries it produced.
            ///
            /// A delimited value can expand into several entries.
            ///
            /// @param[in] s The result to add.
            /// @param[out] results_added How many entries were appended.
            /// @return A pointer to this option, for chaining.
            auto add_result(std::string s, int &results_added) -> option_t *
            {
                results_added = _add_result(std::move(s), results_);
                current_option_state_ = option_state_t::parsing;
                return this;
            }

            /// @brief Adds several raw results, resetting the parse state.
            ///
            /// @param s The results to add.
            /// @return A pointer to this option, for chaining.
            auto add_result(std::vector<std::string> s) -> option_t *
            {
                current_option_state_ = option_state_t::parsing;
                for (auto &str : s)
                {
                    _add_result(std::move(str), results_);
                }
                return this;
            }

            /// @brief Returns the raw results, before validation or reduction.
            ///
            /// @return The raw results.
            [[nodiscard]] auto results() const -> const results_t &
            {
                return results_;
            }

            /// @brief Returns the results after validation and reduction.
            ///
            /// Runs whichever stages have not yet run, without changing the option's
            /// recorded state.
            ///
            /// @return The processed results.
            [[nodiscard]] auto reduced_results() const -> results_t
            {
                results_t res = proc_results_.empty() ? results_ : proc_results_;
                if (current_option_state_ < option_state_t::reduced)
                {
                    if (current_option_state_ == option_state_t::parsing)
                    {
                        res = results_;
                        _validate_results(res);
                    }
                    if (!res.empty())
                    {
                        results_t extra;
                        _reduce_results(extra, res);
                        if (!extra.empty())
                        {
                            res = std::move(extra);
                        }
                    }
                }
                return res;
            }

            /// @brief Converts the results into a value of type @p T.
            ///
            /// Falls back to the printed default when nothing was collected, and to a
            /// value-initialised result when there is no default either.
            ///
            /// @tparam T The type to convert to.
            /// @param[out] output The value to fill.
            /// @throws cli::conversion_error_t If the results cannot be converted.
            template <typename T> auto results(T &output) const -> void
            {
                bool retval = false;
                if (current_option_state_ >= option_state_t::reduced || (results_.size() == 1 && validators_.empty()))
                {
                    const results_t &res = (proc_results_.empty()) ? results_ : proc_results_;
                    if (!res.empty())
                    {
                        retval = detail::lexical_conversion<T, T>(res, output);
                    }
                    else
                    {
                        results_t res2;
                        res2.emplace_back();
                        proc_results_ = std::move(res2);
                        retval = detail::lexical_conversion<T, T>(proc_results_, output);
                    }
                }
                else
                {
                    results_t res;
                    if (results_.empty())
                    {
                        if (!default_str_.empty())
                        {
                            _add_result(std::string(default_str_), res);
                            _validate_results(res);
                            results_t extra;
                            _reduce_results(extra, res);
                            if (!extra.empty())
                            {
                                res = std::move(extra);
                            }
                        }
                        else
                        {
                            res.emplace_back();
                        }
                    }
                    else
                    {
                        res = reduced_results();
                    }
                    proc_results_ = std::move(res);
                    retval = detail::lexical_conversion<T, T>(proc_results_, output);
                }
                if (!retval)
                {
                    throw conversion_error_t(get_name(), results_);
                }
            }

            /// @brief Converts the results and returns them.
            ///
            /// @tparam T The type to convert to.
            /// @return The converted value.
            /// @throws cli::conversion_error_t If the results cannot be converted.
            template <typename T> [[nodiscard]] auto as() const -> T
            {
                T output;
                results(output);
                return output;
            }

            /// @brief Reports whether the callback has already run.
            ///
            /// @return `true` if the callback has run.
            [[nodiscard]] auto get_callback_run() const -> bool
            {
                return (current_option_state_ == option_state_t::callback_run);
            }

            ///@}
            /// @name Type presentation
            ///@{

            /// @brief Sets the callable producing the type name shown in help output.
            ///
            /// @param typefun Produces the type name.
            /// @return A pointer to this option, for chaining.
            auto type_name_fn(std::function<std::string()> typefun) -> option_t *
            {
                type_name_ = std::move(typefun);
                return this;
            }

            /// @brief Sets a fixed type name for help output.
            ///
            /// @param typeval The type name.
            /// @return A pointer to this option, for chaining.
            auto type_name(std::string typeval) -> option_t *
            {
                type_name_fn([val = std::move(typeval)] { return val; });
                return this;
            }

            /// @brief Sets how many values one appearance consumes.
            ///
            /// A negative value fixes the per-appearance count and makes the total
            /// unbounded. A count of zero makes the option optional.
            ///
            /// @param option_type_size The value count.
            /// @return A pointer to this option, for chaining.
            auto type_size(int option_type_size) -> option_t *
            {
                if (option_type_size < 0)
                {
                    type_size_max_ = -option_type_size;
                    type_size_min_ = -option_type_size;
                    expected_max_ = detail::expected_max_vector_size;
                }
                else
                {
                    type_size_max_ = option_type_size;
                    if (type_size_max_ < detail::expected_max_vector_size)
                    {
                        type_size_min_ = option_type_size;
                    }
                    else
                    {
                        inject_separator_ = true;
                    }
                    if (type_size_max_ == 0)
                    {
                        required_ = false;
                    }
                }
                return this;
            }

            /// @brief Sets the range of values one appearance may consume.
            ///
            /// Negative bounds are treated as their magnitude and make the total
            /// unbounded. The bounds are swapped if given in the wrong order.
            ///
            /// @param option_type_size_min The smallest value count.
            /// @param option_type_size_max The largest value count.
            /// @return A pointer to this option, for chaining.
            auto type_size(int option_type_size_min, int option_type_size_max) -> option_t *
            {
                if (option_type_size_min < 0 || option_type_size_max < 0)
                {
                    expected_max_ = detail::expected_max_vector_size;
                    option_type_size_min = (std::abs)(option_type_size_min);
                    option_type_size_max = (std::abs)(option_type_size_max);
                }
                if (option_type_size_min > option_type_size_max)
                {
                    type_size_max_ = option_type_size_min;
                    type_size_min_ = option_type_size_max;
                }
                else
                {
                    type_size_min_ = option_type_size_min;
                    type_size_max_ = option_type_size_max;
                }
                if (type_size_max_ == 0)
                {
                    required_ = false;
                }
                if (type_size_max_ >= detail::expected_max_vector_size)
                {
                    inject_separator_ = true;
                }
                return this;
            }

            /// @brief Sets whether a separator is inserted between value groups.
            ///
            /// @param value Whether to insert a separator.
            auto inject_separator(bool value = true) -> void
            {
                inject_separator_ = value;
            }

            ///@}
            /// @name Default values
            ///@{

            /// @brief Sets the callable that produces the printed default.
            ///
            /// The callable is not invoked here; call @ref capture_default_str to
            /// evaluate it.
            ///
            /// @param func Produces the printed default.
            /// @return A pointer to this option, for chaining.
            auto default_function(std::function<std::string()> func) -> option_t *
            {
                default_function_ = std::move(func);
                return this;
            }

            /// @brief Evaluates the default function and records the result.
            ///
            /// Does nothing when no default function is set.
            ///
            /// @return A pointer to this option, for chaining.
            auto capture_default_str() -> option_t *
            {
                if (default_function_)
                {
                    default_str_ = default_function_();
                }
                return this;
            }

            /// @brief Sets the printed default directly.
            ///
            /// @param val The default as it should appear in help output.
            /// @return A pointer to this option, for chaining.
            auto default_str(std::string val) -> option_t *
            {
                default_str_ = std::move(val);
                return this;
            }

            /// @brief Sets the default from a typed value, checking that it converts.
            ///
            /// The value is pushed through the option's own conversion and validation
            /// path so that an unusable default is reported at construction rather than
            /// at parse time. The option's results and state are restored afterwards,
            /// including when conversion throws.
            ///
            /// @tparam X The type of the supplied value.
            /// @param val The default value.
            /// @return A pointer to this option, for chaining.
            /// @throws cli::conversion_error_t If the value cannot be converted.
            template <typename X> auto default_val(const X &val) -> option_t *
            {
                std::string val_str = detail::value_string(val);
                const auto old_option_state = current_option_state_;
                results_t old_results {std::move(results_)};
                results_.clear();

                try
                {
                    add_result(val_str);
                    if (run_callback_for_default_ && !trigger_on_result_)
                    {
                        run_callback();
                        current_option_state_ = option_state_t::parsing;
                    }
                    else
                    {
                        _validate_results(results_);
                        current_option_state_ = old_option_state;
                    }
                }
                catch (const conversion_error_t &err)
                {
                    results_ = std::move(old_results);
                    current_option_state_ = old_option_state;
                    throw conversion_error_t(get_name(),
                                             std::string("given default value(\"") + val_str +
                                                 "\") produces an error : " + err.what());
                }
                catch (const error_t &)
                {
                    results_ = std::move(old_results);
                    current_option_state_ = old_option_state;
                    throw;
                }

                results_ = std::move(old_results);
                default_str_ = std::move(val_str);
                return this;
            }

            /// @brief Returns the type name shown in help output.
            ///
            /// The generated type name with each validator's description appended,
            /// separated by colons.
            ///
            /// @return The rendered type name.
            [[nodiscard]] auto get_type_name() const -> std::string
            {
                std::string full_type_name = type_name_();
                if (!validators_.empty())
                {
                    for (const auto &validator : validators_)
                    {
                        const std::string vtype = validator->get_description();
                        if (!vtype.empty())
                        {
                            full_type_name += ":" + vtype;
                        }
                    }
                }
                return full_type_name;
            }

            ///@}

        private:
            /// @brief Scans sibling options for a name that collides with this one.
            ///
            /// @tparam T The application type holding the sibling options.
            /// @return The colliding name, or an empty string if there is none.
            template <typename T> auto _find_name_conflict() -> std::string
            {
                auto *parent = static_cast<T *>(parent_);
                for (const option_ptr_t &opt : parent->options_)
                {
                    if (opt.get() == this)
                    {
                        continue;
                    }
                    const auto &omatch = opt->matching_name(*this);
                    if (!omatch.empty())
                    {
                        return omatch;
                    }
                }
                return {};
            }

            /// @brief Runs every validator across a result set.
            ///
            /// Which validator applies to which result depends on the per-appearance
            /// value count. When more results arrived than are wanted and the policy
            /// keeps the tail, the index starts negative so that the surplus leading
            /// results are skipped.
            ///
            /// @param[in,out] res The results to validate; transforms rewrite them.
            /// @throws cli::validation_error_t If any validator rejects a value.
            auto _validate_results(results_t &res) const -> void
            {
                if (validators_.empty())
                {
                    return;
                }

                if (type_size_max_ > 1)
                {
                    int index = 0;
                    if (get_items_expected_max() < static_cast<int>(res.size()) &&
                        (multi_option_policy_ == multi_option_policy_t::take_last ||
                         multi_option_policy_ == multi_option_policy_t::reverse))
                    {
                        index = get_items_expected_max() - static_cast<int>(res.size());
                    }
                    for (std::string &result : res)
                    {
                        if (detail::is_separator(result) && type_size_max_ != type_size_min_ && index >= 0)
                        {
                            index = 0;
                            continue;
                        }
                        const auto err_msg = _validate(result, (index >= 0) ? (index % type_size_max_) : index);
                        if (!err_msg.empty())
                        {
                            throw validation_error_t(get_name(), err_msg);
                        }
                        ++index;
                    }
                }
                else
                {
                    int index = 0;
                    if (expected_max_ < static_cast<int>(res.size()) &&
                        (multi_option_policy_ == multi_option_policy_t::take_last ||
                         multi_option_policy_ == multi_option_policy_t::reverse))
                    {
                        index = expected_max_ - static_cast<int>(res.size());
                    }
                    for (std::string &result : res)
                    {
                        const auto err_msg = _validate(result, index);
                        ++index;
                        if (!err_msg.empty())
                        {
                            throw validation_error_t(get_name(), err_msg);
                        }
                    }
                }
            }

            /// @brief Applies the multi-option policy to a result set.
            ///
            /// Leaves @p out empty when the results are already acceptable as they
            /// stand; callers treat an empty output as "use the original". A lone
            /// `"{}"` gains a `"%%"` separator so that an explicitly empty container
            /// is distinguishable from an absent one.
            ///
            /// @param[out] out The reduced results, or empty if no reduction applies.
            /// @param[in] original The results to reduce.
            /// @throws cli::argument_mismatch_t Under @ref multi_option_policy_t::reject,
            /// if the count falls outside the accepted range.
            auto _reduce_results(results_t &out, const results_t &original) const -> void
            {
                out.clear();

                const auto trimmed_size = [&] {
                    return std::min<std::size_t>(static_cast<std::size_t>(std::max<int>(get_items_expected_max(), 1)),
                                                 original.size());
                };

                switch (multi_option_policy_)
                {
                case multi_option_policy_t::take_all:
                    break;

                case multi_option_policy_t::take_last: {
                    const std::size_t trim_size = trimmed_size();
                    if (original.size() != trim_size)
                    {
                        out.assign(original.end() - static_cast<results_t::difference_type>(trim_size), original.end());
                    }
                }
                break;

                case multi_option_policy_t::reverse: {
                    const std::size_t trim_size = trimmed_size();
                    if (original.size() != trim_size || trim_size > 1)
                    {
                        out.assign(original.end() - static_cast<results_t::difference_type>(trim_size), original.end());
                    }
                    std::ranges::reverse(out);
                }
                break;

                case multi_option_policy_t::take_first: {
                    const std::size_t trim_size = trimmed_size();
                    if (original.size() != trim_size)
                    {
                        out.assign(original.begin(),
                                   original.begin() + static_cast<results_t::difference_type>(trim_size));
                    }
                }
                break;

                case multi_option_policy_t::join:
                    if (results_.size() > 1)
                    {
                        out.push_back(detail::join(original, std::string(1, (delimiter_ == '\0') ? '\n' : delimiter_)));
                    }
                    break;

                case multi_option_policy_t::sum:
                    out.push_back(detail::sum_string_vector(original));
                    break;

                case multi_option_policy_t::reject:
                default: {
                    auto num_min = static_cast<std::size_t>(get_items_expected_min());
                    auto num_max = static_cast<std::size_t>(get_items_expected_max());
                    if (num_min == 0)
                    {
                        num_min = 1;
                    }
                    if (num_max == 0)
                    {
                        num_max = 1;
                    }
                    if (original.size() < num_min)
                    {
                        throw argument_mismatch_t::at_least(get_name(), static_cast<int>(num_min), original.size());
                    }
                    if (original.size() > num_max)
                    {
                        if (original.size() == 2 && num_max == 1 && original[1] == "%%" && original[0] == "{}")
                        {
                            out = original;
                        }
                        else
                        {
                            throw argument_mismatch_t::at_most(get_name(), static_cast<int>(num_max), original.size());
                        }
                    }
                    break;
                }
                }

                if (out.empty())
                {
                    if (original.size() == 1 && original[0] == "{}" && get_items_expected_min() > 0)
                    {
                        out.emplace_back("{}");
                        out.emplace_back("%%");
                    }
                }
                else if (out.size() == 1 && out[0] == "{}" && get_items_expected_min() > 0)
                {
                    out.emplace_back("%%");
                }
            }

            /// @brief Runs the validators that apply to one value.
            ///
            /// A validator with an application index of `-1` applies to every value;
            /// otherwise it applies only at its own index. Stops at the first failure.
            /// An empty value is accepted without checking when the option accepts
            /// zero values.
            ///
            /// @param[in,out] result The value to check; transforms rewrite it.
            /// @param[in] index Which value this is.
            /// @return An empty string on success, otherwise the failure message.
            auto _validate(std::string &result, int index) const -> std::string
            {
                std::string err_msg;
                if (result.empty() && expected_min_ == 0)
                {
                    return err_msg;
                }
                for (const auto &validator : validators_)
                {
                    const auto v = validator->get_application_index();
                    if (v == -1 || v == index)
                    {
                        try
                        {
                            err_msg = (*validator)(result);
                        }
                        catch (const validation_error_t &err)
                        {
                            err_msg = err.what();
                        }
                        if (!err_msg.empty())
                        {
                            break;
                        }
                    }
                }
                return err_msg;
            }

            /// @brief Appends one raw result, expanding it if necessary.
            ///
            /// Three shapes are recognised, in order: a doubled `[[...]]` form, which
            /// marks a nested array and is collapsed back to a single `[...]`; a
            /// bracketed `[a,b,c]` form, which is split and added element by element;
            /// and a delimited value, which is split on the option's delimiter.
            /// Anything else is appended as it stands.
            ///
            /// @param[in] result The value to append; moved from.
            /// @param[out] res The result set to append to.
            /// @return How many entries were appended.
            auto _add_result(std::string &&result, std::vector<std::string> &res) const -> int
            {
                int result_count = 0;

                if (result.size() >= 4 && result[0] == '[' && result[1] == '[' && result.back() == ']' &&
                    (*(result.end() - 2) == ']'))
                {
                    std::string nstrs {'['};
                    bool duplicated {true};
                    for (std::size_t ii = 2; ii < result.size() - 2; ii += 2)
                    {
                        if (result[ii] == result[ii + 1])
                        {
                            nstrs.push_back(result[ii]);
                        }
                        else
                        {
                            duplicated = false;
                            break;
                        }
                    }
                    if (duplicated)
                    {
                        nstrs.push_back(']');
                        res.push_back(std::move(nstrs));
                        ++result_count;
                        return result_count;
                    }
                }

                if ((allow_extra_args_ || get_expected_max() > 1 || get_type_size() > 1) && !result.empty() &&
                    result.front() == '[' && result.back() == ']')
                {
                    result.pop_back();
                    result.erase(result.begin());
                    for (auto &var : detail::split_up(result, ','))
                    {
                        if (!var.empty())
                        {
                            result_count += _add_result(std::move(var), res);
                        }
                    }
                    return result_count;
                }

                if (delimiter_ == '\0')
                {
                    res.push_back(std::move(result));
                    ++result_count;
                }
                else
                {
                    if ((result.find_first_of(delimiter_) != std::string::npos))
                    {
                        for (const auto &var : detail::split(result, delimiter_))
                        {
                            if (!var.empty())
                            {
                                res.push_back(var);
                                ++result_count;
                            }
                        }
                    }
                    else
                    {
                        res.push_back(std::move(result));
                        ++result_count;
                    }
                }
                return result_count;
            }
    };

} // namespace cli
