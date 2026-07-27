/// @file
/// @brief Declarations for the help formatters.
///
/// @ref cli::formatter_base_t holds the layout settings and the labels;
/// @ref cli::formatter_t declares the overridable pieces that assemble a help
/// page. The bodies live in the `formatter` partition, which is separate so that
/// `app_t` can hold a formatter without a circular dependency.
///
/// To customise help output, derive from @ref cli::formatter_t and override the
/// pieces you care about, or wrap a lambda in @ref cli::formatter_lambda_t.

export module cli11:formatter_fwd;

import std;
import :string_tools;

export namespace cli
{

    class option_t;
    class app_t;

    /// @brief How much detail a help request wants.
    enum class app_format_mode_t : std::uint8_t
    {
        normal, ///< The normal, detailed help.
        all,    ///< A fully expanded help.
        sub,    ///< Used when printed as part of an expanded subcommand.
    };

    /// @brief Layout settings and labels shared by every formatter.
    class formatter_base_t
    {
        protected:
            /// @name Options
            ///@{

            /// @brief The width of the left column (options, flags, subcommands).
            std::size_t column_width_ {30};

            /// @brief The alignment ratio for long options within the left column.
            float long_option_alignment_ratio_ {1 / 3.f};

            /// @brief The width of the right column (descriptions).
            std::size_t right_column_width_ {65};

            /// @brief The width of the description paragraph at the top of the help.
            std::size_t description_paragraph_width_ {80};

            /// @brief The width of the footer paragraph.
            std::size_t footer_paragraph_width_ {80};

            /// @brief Whether the description paragraph is reflowed.
            bool enable_description_formatting_ {true};

            /// @brief Whether the footer paragraph is reflowed.
            bool enable_footer_formatting_ {true};

            /// @brief Whether option default values are printed.
            bool enable_option_defaults_ {true};

            /// @brief Whether option type names are printed.
            bool enable_option_type_names_ {true};

            /// @brief Whether default flag values are printed.
            bool enable_default_flag_values_ {true};

            /// @brief User overrides for the section labels.
            ///
            /// Uses a transparent comparator so lookups accept `std::string_view`
            /// without allocating.
            std::map<std::string, std::string, std::less<>> labels_ {};

            /// @brief The label used when no override is set.
            ///
            /// @param key The label to resolve.
            /// @return @p key unchanged.
            [[nodiscard]] static auto default_label(std::string_view key) -> std::string
            {
                return std::string {key};
            }

            ///@}
            /// @name Basics
            ///@{

        public:
            formatter_base_t() = default;
            formatter_base_t(const formatter_base_t &) = default;
            formatter_base_t(formatter_base_t &&) = default;
            auto operator=(const formatter_base_t &) -> formatter_base_t & = default;
            auto operator=(formatter_base_t &&) -> formatter_base_t & = default;

            virtual ~formatter_base_t() noexcept = default;

            /// @brief Assembles a complete help page.
            ///
            /// @param app The application to describe.
            /// @param name The name to present the application under.
            /// @param mode How much detail to include.
            /// @return The rendered help page.
            [[nodiscard]] virtual auto make_help(const app_t *app, std::string name, app_format_mode_t mode) const
                -> std::string = 0;

            ///@}
            /// @name Setters
            ///@{

            /// @brief Overrides a section label.
            ///
            /// @param key The label to override.
            /// @param val The replacement text.
            auto label(std::string key, std::string val) -> void
            {
                labels_.insert_or_assign(std::move(key), std::move(val));
            }

            /// @brief Sets the width of the left column.
            ///
            /// @param val The new width, in characters.
            auto column_width(std::size_t val) -> void
            {
                column_width_ = val;
            }

            /// @brief Sets the alignment ratio for long options in the left column.
            ///
            /// Values outside `[0, 1]` are folded back into range.
            ///
            /// @param ratio The new ratio.
            auto long_option_alignment_ratio(float ratio) -> void
            {
                long_option_alignment_ratio_ = (ratio >= 0.0f) ? ((ratio <= 1.0f) ? ratio : 1.0f / ratio)
                                                               : ((ratio < -1.0f) ? 1.0f / (-ratio) : -ratio);
            }

            /// @brief Sets the width of the right column.
            ///
            /// @param val The new width, in characters.
            auto right_column_width(std::size_t val) -> void
            {
                right_column_width_ = val;
            }

            /// @brief Sets the width of the description paragraph.
            ///
            /// @param val The new width, in characters.
            auto description_paragraph_width(std::size_t val) -> void
            {
                description_paragraph_width_ = val;
            }

            /// @brief Sets the width of the footer paragraph.
            ///
            /// @param val The new width, in characters.
            auto footer_paragraph_width(std::size_t val) -> void
            {
                footer_paragraph_width_ = val;
            }

            /// @brief Enables or disables reflowing of the description paragraph.
            ///
            /// @param value Whether to reflow.
            auto enable_description_formatting(bool value = true) -> void
            {
                enable_description_formatting_ = value;
            }

            /// @brief Enables or disables reflowing of the footer paragraph.
            ///
            /// @param value Whether to reflow.
            auto enable_footer_formatting(bool value = true) -> void
            {
                enable_footer_formatting_ = value;
            }

            /// @brief Enables or disables printing of option default values.
            ///
            /// @param value Whether to print defaults.
            auto enable_option_defaults(bool value = true) -> void
            {
                enable_option_defaults_ = value;
            }

            /// @brief Enables or disables printing of option type names.
            ///
            /// @param value Whether to print type names.
            auto enable_option_type_names(bool value = true) -> void
            {
                enable_option_type_names_ = value;
            }

            /// @brief Enables or disables printing of default flag values.
            ///
            /// @param value Whether to print default flag values.
            auto enable_default_flag_values(bool value = true) -> void
            {
                enable_default_flag_values_ = value;
            }

            ///@}
            /// @name Getters
            ///@{

            /// @brief Resolves a section label, applying any override.
            ///
            /// @param key The label to resolve.
            /// @return The override if one is set, otherwise @p key unchanged.
            [[nodiscard]] auto get_label(std::string_view key) const -> std::string
            {
                const auto it = labels_.find(key);
                return it != labels_.end() ? it->second : default_label(key);
            }

            /// @brief Returns the width of the left column.
            ///
            /// @return The width, in characters.
            [[nodiscard]] auto get_column_width() const -> std::size_t
            {
                return column_width_;
            }

            /// @brief Returns the width of the right column.
            ///
            /// @return The width, in characters.
            [[nodiscard]] auto get_right_column_width() const -> std::size_t
            {
                return right_column_width_;
            }

            /// @brief Returns the width of the description paragraph.
            ///
            /// @return The width, in characters.
            [[nodiscard]] auto get_description_paragraph_width() const -> std::size_t
            {
                return description_paragraph_width_;
            }

            /// @brief Returns the width of the footer paragraph.
            ///
            /// @return The width, in characters.
            [[nodiscard]] auto get_footer_paragraph_width() const -> std::size_t
            {
                return footer_paragraph_width_;
            }

            /// @brief Returns the alignment ratio for long options.
            ///
            /// @return The ratio.
            [[nodiscard]] auto get_long_option_alignment_ratio() const -> float
            {
                return long_option_alignment_ratio_;
            }

            /// @brief Reports whether the description paragraph is reflowed.
            ///
            /// @return `true` if reflowing is enabled.
            [[nodiscard]] auto is_description_paragraph_formatting_enabled() const -> bool
            {
                return enable_description_formatting_;
            }

            /// @brief Reports whether the footer paragraph is reflowed.
            ///
            /// @return `true` if reflowing is enabled.
            [[nodiscard]] auto is_footer_paragraph_formatting_enabled() const -> bool
            {
                return enable_footer_formatting_;
            }

            /// @brief Reports whether option default values are printed.
            ///
            /// @return `true` if defaults are printed.
            [[nodiscard]] auto is_option_defaults_enabled() const -> bool
            {
                return enable_option_defaults_;
            }

            /// @brief Reports whether option type names are printed.
            ///
            /// @return `true` if type names are printed.
            [[nodiscard]] auto is_option_type_names_enabled() const -> bool
            {
                return enable_option_type_names_;
            }

            /// @brief Reports whether default flag values are printed.
            ///
            /// @return `true` if default flag values are printed.
            [[nodiscard]] auto is_default_flag_values_enabled() const -> bool
            {
                return enable_default_flag_values_;
            }

            ///@}
    };

    /// @brief A formatter that delegates the whole help page to a callable.
    class formatter_lambda_t final : public formatter_base_t
    {
            /// @brief The callable that renders the help page.
            ///
            /// @note This wants to be `std::copyable_function<... const>`, which fixes
            /// the const-correctness of the call operator, but libc++ does not yet
            /// implement it.
            using funct_t = std::function<std::string(const app_t *, std::string, app_format_mode_t)>;

            /// @brief The wrapped callable.
            funct_t lambda_;

        public:
            /// @brief Wraps a callable as a formatter.
            ///
            /// @param funct The callable that renders the help page.
            explicit formatter_lambda_t(funct_t funct) : lambda_(std::move(funct))
            {
            }

            ~formatter_lambda_t() noexcept override = default;

            /// @brief Renders the help page by invoking the wrapped callable.
            ///
            /// @param app The application to describe.
            /// @param name The name to present the application under.
            /// @param mode How much detail to include.
            /// @return The rendered help page.
            [[nodiscard]] auto make_help(const app_t *app, std::string name, app_format_mode_t mode) const
                -> std::string override
            {
                return lambda_(app, std::move(name), mode);
            }
    };

    /// @brief The default help formatter.
    ///
    /// Each piece of the help page is a separate virtual, so a derived formatter
    /// can replace one section without reimplementing the rest.
    class formatter_t : public formatter_base_t
    {
        public:
            formatter_t() = default;
            formatter_t(const formatter_t &) = default;
            formatter_t(formatter_t &&) = default;
            auto operator=(const formatter_t &) -> formatter_t & = default;
            auto operator=(formatter_t &&) -> formatter_t & = default;

            /// @name Overridables
            ///@{

            /// @brief Renders one named group of options.
            ///
            /// @param group The group name.
            /// @param is_positional Whether the group holds positionals.
            /// @param opts The options in the group.
            /// @return The rendered group.
            [[nodiscard]] virtual auto make_group(std::string group,
                                                  bool is_positional,
                                                  std::vector<const option_t *> opts) const -> std::string;

            /// @brief Renders the positional arguments section.
            ///
            /// @param app The application to describe.
            /// @return The rendered section.
            [[nodiscard]] virtual auto make_positionals(const app_t *app) const -> std::string;

            /// @brief Renders every option group.
            ///
            /// @param app The application to describe.
            /// @param mode How much detail to include.
            /// @return The rendered sections.
            [[nodiscard]] auto make_groups(const app_t *app, app_format_mode_t mode) const -> std::string;

            /// @brief Renders the subcommand list.
            ///
            /// @param app The application to describe.
            /// @param mode How much detail to include.
            /// @return The rendered section.
            [[nodiscard]] virtual auto make_subcommands(const app_t *app, app_format_mode_t mode) const -> std::string;

            /// @brief Renders a single subcommand entry.
            ///
            /// @param sub The subcommand to describe.
            /// @return The rendered entry.
            [[nodiscard]] virtual auto make_subcommand(const app_t *sub) const -> std::string;

            /// @brief Renders a subcommand expanded in full.
            ///
            /// @param sub The subcommand to describe.
            /// @param mode How much detail to include.
            /// @return The rendered entry.
            [[nodiscard]] virtual auto make_expanded(const app_t *sub, app_format_mode_t mode) const -> std::string;

            /// @brief Renders the footer.
            ///
            /// @param app The application to describe.
            /// @return The rendered footer.
            [[nodiscard]] virtual auto make_footer(const app_t *app) const -> std::string;

            /// @brief Renders the description paragraph.
            ///
            /// @param app The application to describe.
            /// @return The rendered description.
            [[nodiscard]] virtual auto make_description(const app_t *app) const -> std::string;

            /// @brief Renders the usage line.
            ///
            /// @param app The application to describe.
            /// @param name The name to present the application under.
            /// @return The rendered usage line.
            [[nodiscard]] virtual auto make_usage(const app_t *app, std::string name) const -> std::string;

            /// @brief Assembles the complete help page.
            ///
            /// @param app The application to describe.
            /// @param name The name to present the application under.
            /// @param mode How much detail to include.
            /// @return The rendered help page.
            [[nodiscard]] auto make_help(const app_t *app, std::string name, app_format_mode_t mode) const
                -> std::string override;

            ///@}
            /// @name Options
            ///@{

            /// @brief Renders a single option entry.
            ///
            /// @param opt The option to describe.
            /// @param is_positional Whether the option is a positional.
            /// @return The rendered entry.
            [[nodiscard]] virtual auto make_option(const option_t *opt, bool is_positional) const -> std::string;

            /// @brief Renders the name column of an option entry.
            ///
            /// @param opt The option to describe.
            /// @param is_positional Whether the option is a positional.
            /// @return The rendered name column.
            [[nodiscard]] virtual auto make_option_name(const option_t *opt, bool is_positional) const -> std::string;

            /// @brief Renders the qualifiers that follow an option's name.
            ///
            /// @param opt The option to describe.
            /// @return The rendered qualifiers.
            [[nodiscard]] virtual auto make_option_opts(const option_t *opt) const -> std::string;

            /// @brief Renders an option's description column.
            ///
            /// @param opt The option to describe.
            /// @return The rendered description.
            [[nodiscard]] virtual auto make_option_desc(const option_t *opt) const -> std::string;

            /// @brief Renders how an option appears in the usage line.
            ///
            /// @param opt The option to describe.
            /// @return The rendered usage fragment.
            [[nodiscard]] virtual auto make_option_usage(const option_t *opt) const -> std::string;

            ///@}
    };

} // namespace cli
