/// @file
/// @brief Definitions for the default help formatter.
///
/// The declarations live in the `formatter_fwd` partition; the bodies are here
/// because they need the full definitions of @ref cli::app_t and
/// @ref cli::option_t, which would otherwise form a cycle.
///
/// The page is assembled from the bottom up: @ref cli::formatter_t::make_option
/// renders one entry, @ref cli::formatter_t::make_group renders a labelled block
/// of them, and @ref cli::formatter_t::make_help stitches together the usage
/// line, description, groups, subcommands, and footer. Overriding any one of
/// them replaces just that piece.

export module cli11:formatter;

import std;
import :string_tools;
import :option;
import :formatter_fwd;
import :app;

export namespace cli
{

    auto formatter_t::make_group(std::string group, bool is_positional, std::vector<const option_t *> opts) const
        -> std::string
    {
        std::ostringstream out;

        out << "\n" << group << ":\n";
        for (const option_t *opt : opts)
        {
            out << make_option(opt, is_positional);
        }

        return out.str();
    }

    auto formatter_t::make_positionals(const app_t *app) const -> std::string
    {
        std::vector<const option_t *> opts =
            app->get_options([](const option_t *opt) { return !opt->get_group().empty() && opt->get_positional(); });

        if (opts.empty())
        {
            return {};
        }

        return make_group(get_label("POSITIONALS"), true, opts);
    }

    auto formatter_t::make_groups(const app_t *app, app_format_mode_t mode) const -> std::string
    {
        std::ostringstream out;
        const std::vector<std::string> groups = app->get_groups();

        for (const std::string &group : groups)
        {
            std::vector<const option_t *> opts = app->get_options([app, mode, &group](const option_t *opt) {
                return opt->get_group() == group                    // must be in the right group
                       && opt->nonpositional()                      // must not be a positional
                       && (mode != app_format_mode_t::sub           // when rendering as a subcommand,
                           || (app->get_help_ptr() != opt           // skip the help option
                               && app->get_help_all_ptr() != opt)); // and the expanded-help option
            });
            if (!group.empty() && !opts.empty())
            {
                out << make_group(group, false, opts);
            }
        }

        return out.str();
    }

    auto formatter_t::make_description(const app_t *app) const -> std::string
    {
        std::string desc = app->get_description();
        const auto min_options = app->get_require_option_min();
        const auto max_options = app->get_require_option_max();

        if (app->get_required())
        {
            desc += " " + get_label("REQUIRED") + " ";
        }

        if (min_options > 0)
        {
            if (max_options == min_options)
            {
                desc += " \n[Exactly " + std::to_string(min_options) + " of the following options are required]";
            }
            else if (max_options > 0)
            {
                desc += " \n[Between " + std::to_string(min_options) + " and " + std::to_string(max_options) +
                        " of the following options are required]";
            }
            else
            {
                desc += " \n[At least " + std::to_string(min_options) + " of the following options are required]";
            }
        }
        else if (max_options > 0)
        {
            desc += " \n[At most " + std::to_string(max_options) + " of the following options are allowed]";
        }

        return (!desc.empty()) ? desc + "\n\n" : std::string {};
    }

    auto formatter_t::make_usage(const app_t *app, std::string name) const -> std::string
    {
        const std::string usage = app->get_usage();
        if (!usage.empty())
        {
            return usage + "\n\n";
        }

        std::ostringstream out;
        out << '\n';

        if (name.empty())
        {
            out << get_label("Usage") << ':';
        }
        else
        {
            out << name;
        }

        // An OPTIONS badge stands in for the whole non-positional set.
        const std::vector<const option_t *> non_pos_options =
            app->get_options([](const option_t *opt) { return opt->nonpositional(); });
        if (!non_pos_options.empty())
        {
            out << " [" << get_label("OPTIONS") << "]";
        }

        // Positionals are named individually, since order matters.
        const std::vector<const option_t *> positionals =
            app->get_options([](const option_t *opt) { return opt->get_positional(); });

        if (!positionals.empty())
        {
            std::vector<std::string> positional_names;
            positional_names.reserve(positionals.size());
            for (const auto *opt : positionals)
            {
                positional_names.push_back(make_option_usage(opt));
            }

            out << " " << detail::join(positional_names, " ");
        }

        // A trailing marker, bracketed when subcommands are optional.
        if (!app->get_subcommands(
                    [](const app_t *subc) { return ((!subc->get_disabled()) && (!subc->get_name().empty())); })
                 .empty())
        {
            out << ' ' << (app->get_require_subcommand_min() == 0 ? "[" : "")
                << get_label(app->get_require_subcommand_max() == 1 ? "SUBCOMMAND" : "SUBCOMMANDS")
                << (app->get_require_subcommand_min() == 0 ? "]" : "");
        }

        out << "\n\n";

        return out.str();
    }

    auto formatter_t::make_footer(const app_t *app) const -> std::string
    {
        const std::string footer = app->get_footer();
        if (footer.empty())
        {
            return std::string {};
        }
        return '\n' + footer + '\n';
    }

    auto formatter_t::make_help(const app_t *app, std::string name, app_format_mode_t mode) const -> std::string
    {
        // Forwarded rather than inlined so that a subcommand carrying its own
        // formatter gets to render itself.
        if (mode == app_format_mode_t::sub)
        {
            return make_expanded(app, mode);
        }

        std::ostringstream out;
        if ((app->get_name().empty()) && (app->get_parent() != nullptr))
        {
            if (app->get_group() != "SUBCOMMANDS")
            {
                out << app->get_group() << ':';
            }
        }

        if (is_description_paragraph_formatting_enabled())
        {
            detail::stream_out_as_paragraph(out, make_description(app), description_paragraph_width_, "");
        }
        else
        {
            out << make_description(app) << '\n';
        }

        out << make_usage(app, name);
        out << make_positionals(app);
        out << make_groups(app, mode);
        out << make_subcommands(app, mode);

        const std::string footer_string = make_footer(app);
        if (is_footer_paragraph_formatting_enabled())
        {
            detail::stream_out_as_paragraph(out, footer_string, footer_paragraph_width_);
        }
        else
        {
            out << footer_string;
        }

        return out.str();
    }

    auto formatter_t::make_subcommands(const app_t *app, app_format_mode_t mode) const -> std::string
    {
        std::ostringstream out;

        const std::vector<const app_t *> subcommands = app->get_subcommands({});

        // Collect the group names in definition order. A nameless subcommand is an
        // option group, so it is expanded in place rather than listed; a leading '+'
        // on its group marks it as already merged into the parent.
        std::vector<std::string> subcmd_groups_seen;
        for (const app_t *com : subcommands)
        {
            if (com->get_name().empty())
            {
                if (!com->get_group().empty() && com->get_group().front() != '+')
                {
                    out << make_expanded(com, mode);
                }
                continue;
            }

            const std::string group_key = com->get_group();
            if (group_key.empty())
            {
                continue;
            }
            const std::string lower_group_key = detail::to_lower(group_key);
            if (std::ranges::none_of(subcmd_groups_seen, [&lower_group_key](const std::string &a) {
                    return detail::to_lower(a) == lower_group_key;
                }))
            {
                subcmd_groups_seen.push_back(group_key);
            }
        }

        for (const std::string &group : subcmd_groups_seen)
        {
            out << '\n' << group << ":\n";
            const std::vector<const app_t *> subcommands_group = app->get_subcommands([&group](const app_t *sub_app) {
                return detail::to_lower(sub_app->get_group()) == detail::to_lower(group);
            });

            for (const app_t *new_com : subcommands_group)
            {
                if (new_com->get_name().empty())
                {
                    continue;
                }
                if (mode != app_format_mode_t::all)
                {
                    out << make_subcommand(new_com);
                }
                else
                {
                    out << new_com->help(new_com->get_name(), app_format_mode_t::sub);
                    out << '\n';
                }
            }
        }

        return out.str();
    }

    auto formatter_t::make_subcommand(const app_t *sub) const -> std::string
    {
        std::ostringstream out;
        const std::string name =
            "  " + sub->get_display_name(true) + (sub->get_required() ? " " + get_label("REQUIRED") : "");

        out << std::setw(static_cast<int>(column_width_)) << std::left << name;
        detail::stream_out_as_paragraph(
            out, sub->get_description(), right_column_width_, std::string(column_width_, ' '), true);
        out << '\n';
        return out.str();
    }

    auto formatter_t::make_expanded(const app_t *sub, app_format_mode_t mode) const -> std::string
    {
        std::ostringstream out;
        out << sub->get_display_name(true) << '\n';

        if (is_description_paragraph_formatting_enabled())
        {
            detail::stream_out_as_paragraph(out, make_description(sub), description_paragraph_width_, "  ");
        }
        else
        {
            out << make_description(sub) << '\n';
        }

        if (sub->get_name().empty() && !sub->get_aliases().empty())
        {
            detail::format_aliases(out, sub->get_aliases(), column_width_ + 2);
        }

        out << make_positionals(sub);
        out << make_groups(sub, mode);
        out << make_subcommands(sub, mode);

        std::string footer_string = make_footer(sub);

        // A subcommand that merely inherits its parent's footer should not repeat it.
        if (mode == app_format_mode_t::sub && !footer_string.empty())
        {
            const auto *parent = sub->get_parent();
            const std::string parent_footer = (parent != nullptr) ? make_footer(parent) : std::string {};
            if (footer_string == parent_footer)
            {
                footer_string.clear();
            }
        }

        if (!footer_string.empty())
        {
            if (is_footer_paragraph_formatting_enabled())
            {
                detail::stream_out_as_paragraph(out, footer_string, footer_paragraph_width_);
            }
            else
            {
                out << footer_string;
            }
        }
        return out.str();
    }

    auto formatter_t::make_option(const option_t *opt, bool is_positional) const -> std::string
    {
        std::ostringstream out;

        if (is_positional)
        {
            const std::string left = "  " + make_option_name(opt, true) + make_option_opts(opt);
            const std::string desc = make_option_desc(opt);
            out << std::setw(static_cast<int>(column_width_)) << std::left << left;

            if (!desc.empty())
            {
                bool skip_first_line_prefix = true;
                if (left.length() >= column_width_)
                {
                    out << '\n';
                    skip_first_line_prefix = false;
                }
                detail::stream_out_as_paragraph(
                    out, desc, right_column_width_, std::string(column_width_, ' '), skip_first_line_prefix);
            }
        }
        else
        {
            const std::string names_combined = make_option_name(opt, false);
            const std::string opts = make_option_opts(opt);
            const std::string desc = make_option_desc(opt);

            // Split the names apart so that short and long forms can be aligned in
            // separate sub-columns.
            const auto names = detail::split(names_combined, ',');
            std::vector<std::string> short_name_list;
            std::vector<std::string> long_name_list;
            for (const std::string &name : names)
            {
                if (name.find("--", 0) != std::string::npos)
                {
                    long_name_list.push_back(name);
                }
                else
                {
                    short_name_list.push_back(name);
                }
            }

            std::string short_names = detail::join(short_name_list, ", ");
            std::string long_names = detail::join(long_name_list, ", ");

            // The short-name sub-column is sized so that long names begin at the
            // configured fraction of the left column.
            const auto short_names_column_width =
                static_cast<int>(static_cast<float>(column_width_) * long_option_alignment_ratio_);
            const auto long_names_column_width = static_cast<int>(column_width_) - short_names_column_width;
            int short_names_oversize = 0;

            if (!short_names.empty())
            {
                short_names = "  " + short_names;
                if (long_names.empty() && !opts.empty())
                {
                    short_names += opts;
                }
                if (!long_names.empty())
                {
                    short_names += ",";
                }
                if (static_cast<int>(short_names.length()) >= short_names_column_width)
                {
                    short_names += " ";
                    short_names_oversize = static_cast<int>(short_names.length()) - short_names_column_width;
                }
                out << std::setw(short_names_column_width) << std::left << short_names;
            }
            else
            {
                out << std::setw(short_names_column_width) << std::left << "";
            }

            // Clamped so that an overlong short-name column cannot drive the long-name
            // width negative.
            short_names_oversize = (std::min)(short_names_oversize, long_names_column_width);
            const auto adjusted_long_names_column_width = long_names_column_width - short_names_oversize;

            if (!long_names.empty())
            {
                if (!opts.empty())
                {
                    long_names += opts;
                }
                if (static_cast<int>(long_names.length()) >= adjusted_long_names_column_width)
                {
                    long_names += " ";
                }
                out << std::setw(adjusted_long_names_column_width) << std::left << long_names;
            }
            else
            {
                out << std::setw(adjusted_long_names_column_width) << std::left << "";
            }

            if (!desc.empty())
            {
                bool skip_first_line_prefix = true;
                if (out.str().length() > column_width_)
                {
                    out << '\n';
                    skip_first_line_prefix = false;
                }
                detail::stream_out_as_paragraph(
                    out, desc, right_column_width_, std::string(column_width_, ' '), skip_first_line_prefix);
            }
        }

        out << '\n';
        return out.str();
    }

    auto formatter_t::make_option_name(const option_t *opt, bool is_positional) const -> std::string
    {
        if (is_positional)
        {
            return opt->get_name(true, false);
        }
        return opt->get_name(false, true, !enable_default_flag_values_);
    }

    auto formatter_t::make_option_opts(const option_t *opt) const -> std::string
    {
        std::ostringstream out;

        // Help output must be stable across runs, and the dependency sets are keyed
        // on pointers, so sort by name before printing.
        const auto print_option_set = [&out](const std::set<option_t *> &options) {
            std::vector<const option_t *> sorted(options.begin(), options.end());
            std::ranges::sort(sorted, {}, [](const option_t *op) { return op->get_name(); });
            for (const option_t *op : sorted)
            {
                out << " " << op->get_name();
            }
        };

        if (!opt->get_option_text().empty())
        {
            out << " " << opt->get_option_text();
        }
        else
        {
            if (opt->get_type_size() != 0)
            {
                if (enable_option_type_names_)
                {
                    if (!opt->get_type_name().empty())
                    {
                        out << " " << get_label(opt->get_type_name());
                    }
                }
                if (enable_option_defaults_)
                {
                    if (!opt->get_default_str().empty())
                    {
                        out << " [" << opt->get_default_str() << "] ";
                    }
                }
                if (opt->get_expected_max() == detail::expected_max_vector_size)
                {
                    out << " ...";
                }
                else if (opt->get_expected_min() > 1)
                {
                    out << " x " << opt->get_expected();
                }
                if (opt->get_required())
                {
                    out << " " << get_label("REQUIRED");
                }
            }
            if (!opt->get_envname().empty())
            {
                out << " (" << get_label("Env") << ":" << opt->get_envname() << ")";
            }
            if (!opt->get_needs().empty())
            {
                out << " " << get_label("Needs") << ":";
                print_option_set(opt->get_needs());
            }
            if (!opt->get_excludes().empty())
            {
                out << " " << get_label("Excludes") << ":";
                print_option_set(opt->get_excludes());
            }
        }
        return out.str();
    }

    auto formatter_t::make_option_desc(const option_t *opt) const -> std::string
    {
        return opt->get_description();
    }

    auto formatter_t::make_option_usage(const option_t *opt) const -> std::string
    {
        // Positionals only; dashed options are represented by the OPTIONS badge.
        std::ostringstream out;
        out << make_option_name(opt, true);

        if (opt->get_expected_max() >= detail::expected_max_vector_size)
        {
            out << "...";
        }
        else if (opt->get_expected_max() > 1)
        {
            out << "(" << opt->get_expected() << "x)";
        }

        return opt->get_required() ? out.str() : "[" + out.str() + "]";
    }

} // namespace cli
