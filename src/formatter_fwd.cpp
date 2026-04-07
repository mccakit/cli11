// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

export module cli11:formatter_fwd;

import std;
import :string_tools;

export namespace CLI {

class Option;
class App;

/// This enum signifies the type of help requested
enum class AppFormatMode : std::uint8_t {
    Normal,  ///< The normal, detailed help
    All,     ///< A fully expanded help
    Sub,     ///< Used when printed as part of expanded subcommand
};

/// This is the minimum requirements to run a formatter.
class FormatterBase {
  protected:
    /// @name Options
    ///@{

    /// The width of the left column (options/flags/subcommands)
    std::size_t column_width_{30};

    /// The alignment ratio for long options within the left column
    float long_option_alignment_ratio_{1 / 3.f};

    /// The width of the right column (description of options/flags/subcommands)
    std::size_t right_column_width_{65};

    /// The width of the description paragraph at the top of the help
    std::size_t description_paragraph_width_{80};

    /// The width of the footer paragraph
    std::size_t footer_paragraph_width_{80};

    /// Options for controlling formatting of the footer and descriptions
    bool enable_description_formatting_{true};
    bool enable_footer_formatting_{true};

    /// Options for controlling the formatting of options
    bool enable_option_defaults_{true};
    bool enable_option_type_names_{true};
    bool enable_default_flag_values_{true};

    /// @brief The labels used for the required help printing (can be changed by user)
    std::map<std::string, std::string> labels_{};

    /// The default user labels when no overrides are set
    static std::string default_label(const std::string &key) { return key; }

    ///@}
    /// @name Basics
    ///@{

  public:
    FormatterBase() = default;
    FormatterBase(const FormatterBase &) = default;
    FormatterBase(FormatterBase &&) = default;
    FormatterBase &operator=(const FormatterBase &) = default;
    FormatterBase &operator=(FormatterBase &&) = default;

    virtual ~FormatterBase() noexcept = default;

    /// This is the main method that puts the help together
    virtual std::string make_help(const App *, std::string, AppFormatMode) const = 0;

    ///@}
    /// @name Setters
    ///@{

    void label(std::string key, std::string val) { labels_[key] = std::move(val); }

    void column_width(std::size_t val) { column_width_ = val; }

    void long_option_alignment_ratio(float ratio) {
        long_option_alignment_ratio_ =
            (ratio >= 0.0f) ? ((ratio <= 1.0f) ? ratio : 1.0f / ratio) : ((ratio < -1.0f) ? 1.0f / (-ratio) : -ratio);
    }

    void right_column_width(std::size_t val) { right_column_width_ = val; }

    void description_paragraph_width(std::size_t val) { description_paragraph_width_ = val; }

    void footer_paragraph_width(std::size_t val) { footer_paragraph_width_ = val; }
    void enable_description_formatting(bool value = true) { enable_description_formatting_ = value; }
    void enable_footer_formatting(bool value = true) { enable_footer_formatting_ = value; }

    void enable_option_defaults(bool value = true) { enable_option_defaults_ = value; }
    void enable_option_type_names(bool value = true) { enable_option_type_names_ = value; }
    void enable_default_flag_values(bool value = true) { enable_default_flag_values_ = value; }
    ///@}
    /// @name Getters
    ///@{

    [[nodiscard]] std::string get_label(std::string key) const {
        auto it = labels_.find(key);
        return it != labels_.end() ? it->second : default_label(key);
    }

    [[nodiscard]] std::size_t get_column_width() const { return column_width_; }

    [[nodiscard]] std::size_t get_right_column_width() const { return right_column_width_; }

    [[nodiscard]] std::size_t get_description_paragraph_width() const { return description_paragraph_width_; }

    [[nodiscard]] std::size_t get_footer_paragraph_width() const { return footer_paragraph_width_; }

    [[nodiscard]] float get_long_option_alignment_ratio() const { return long_option_alignment_ratio_; }

    [[nodiscard]] bool is_description_paragraph_formatting_enabled() const { return enable_description_formatting_; }

    [[nodiscard]] bool is_footer_paragraph_formatting_enabled() const { return enable_footer_formatting_; }

    [[nodiscard]] bool is_option_defaults_enabled() const { return enable_option_defaults_; }

    [[nodiscard]] bool is_option_type_names_enabled() const { return enable_option_type_names_; }

    [[nodiscard]] bool is_default_flag_values_enabled() const { return enable_default_flag_values_; }

    ///@}
};

/// This is a special override class for lambda functions
class FormatterLambda final : public FormatterBase {
    using funct_t = std::function<std::string(const App *, std::string, AppFormatMode)>;

    funct_t lambda_;

  public:
    explicit FormatterLambda(funct_t funct) : lambda_(std::move(funct)) {}

    ~FormatterLambda() noexcept override = default;

    std::string make_help(const App *app, std::string name, AppFormatMode mode) const override {
        return lambda_(app, std::move(name), mode);
    }
};

/// This is the default Formatter for CLI11.
class Formatter : public FormatterBase {
  public:
    Formatter() = default;
    Formatter(const Formatter &) = default;
    Formatter(Formatter &&) = default;
    Formatter &operator=(const Formatter &) = default;
    Formatter &operator=(Formatter &&) = default;

    /// @name Overridables
    ///@{

    [[nodiscard]] virtual std::string
    make_group(std::string group, bool is_positional, std::vector<const Option *> opts) const;

    virtual std::string make_positionals(const App *app) const;

    std::string make_groups(const App *app, AppFormatMode mode) const;

    virtual std::string make_subcommands(const App *app, AppFormatMode mode) const;

    virtual std::string make_subcommand(const App *sub) const;

    virtual std::string make_expanded(const App *sub, AppFormatMode mode) const;

    virtual std::string make_footer(const App *app) const;

    virtual std::string make_description(const App *app) const;

    virtual std::string make_usage(const App *app, std::string name) const;

    std::string make_help(const App *app, std::string name, AppFormatMode mode) const override;

    ///@}
    /// @name Options
    ///@{

    virtual std::string make_option(const Option *, bool) const;

    virtual std::string make_option_name(const Option *, bool) const;

    virtual std::string make_option_opts(const Option *) const;

    virtual std::string make_option_desc(const Option *) const;

    virtual std::string make_option_usage(const Option *opt) const;

    ///@}
};

} // namespace CLI
