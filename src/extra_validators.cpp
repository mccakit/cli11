/// @file
/// @brief The heavier validators: type checks, set membership, and unit parsing.
///
/// These sit apart from the `validators` partition because they are templated on
/// the option's type or on a container of allowed values, and because most carry
/// enough machinery to be worth isolating:
///
/// - @ref cli::type_validator_t checks that a value parses as a given type.
/// - @ref cli::bound_t clamps a value into a range rather than rejecting it.
/// - @ref cli::is_member_t checks a value against a set, optionally case- and
///   underscore-insensitively.
/// - @ref cli::transformer_t and @ref cli::checked_transformer_t map a value
///   through a lookup table.
/// - @ref cli::as_number_with_unit_t and @ref cli::as_size_value_t parse
///   `"10kb"` style values.
///
/// Include this partition only if you need them; `cli11.cpp` re-exports it.

export module cli11:extra_validators;

import std;
import :string_tools;
import :error;
import :validators;
import :type_tools;
import :encoding;

export namespace cli
{

    namespace detail
    {

        /// @brief Requires the value to be a legal IPv4 address.
        ///
        /// Exposed to callers as the @ref cli::valid_ipv4 instance; there is nothing
        /// to configure, so the class itself stays in `detail`.
        class ipv4_validator_t : public validator_t
        {
            public:
                ipv4_validator_t();
        };

    } // namespace detail

    /// @brief Requires the value to parse as a particular type.
    ///
    /// @tparam desired_t The type the value must convert to.
    template <typename desired_t> class type_validator_t : public validator_t
    {
        public:
            /// @brief Constructs the validator with an explicit name.
            ///
            /// @param validator_name The name shown in help output.
            explicit type_validator_t(const std::string &validator_name)
                : validator_t(validator_name, [](std::string &input_string) {
                      using detail::lexical_cast;
                      auto val = desired_t();
                      if (!lexical_cast(input_string, val))
                      {
                          return std::string("Failed parsing ") + input_string + " as a " +
                                 detail::type_name<desired_t>();
                      }
                      return std::string {};
                  })
            {
            }

            /// @brief Constructs the validator, naming it after the type.
            type_validator_t() : type_validator_t(detail::type_name<desired_t>())
            {
            }
    };

    /// @brief Requires the value to parse as a number.
    const type_validator_t<double> number("NUMBER");

    /// @brief Clamps the value into a closed interval instead of rejecting it.
    ///
    /// Unlike @ref cli::range_t, which reports an error, this rewrites the value to
    /// the nearer bound.
    class bound_t : public validator_t
    {
        public:
            /// @brief Clamps to `[min_val, max_val]`.
            ///
            /// The constructor is templated while the class is not, so `bound_t(a, b)`
            /// works without the caller naming the type.
            ///
            /// @tparam T The type the value is converted to before comparison.
            /// @param min_val The lowest permitted value.
            /// @param max_val The highest permitted value.
            template <typename T> bound_t(T min_val, T max_val)
            {
                std::ostringstream out;
                out << detail::type_name<T>() << " bounded to [" << min_val << " - " << max_val << "]";
                description(out.str());

                func_ = [min_val, max_val](std::string &input) {
                    using detail::lexical_cast;
                    T val;
                    const bool converted = lexical_cast(input, val);
                    if (!converted)
                    {
                        return std::string("Value ") + input + " could not be converted";
                    }
                    if (val < min_val)
                    {
                        input = detail::to_string(min_val);
                    }
                    else if (val > max_val)
                    {
                        input = detail::to_string(max_val);
                    }
                    return std::string {};
                };
            }

            /// @brief Clamps to `[0, max_val]`.
            ///
            /// @tparam T The type the value is converted to before comparison.
            /// @param max_val The highest permitted value.
            template <typename T> explicit bound_t(T max_val) : bound_t(static_cast<T>(0), max_val)
            {
            }
    };

    /// @brief Requires the value to be a legal IPv4 address.
    const detail::ipv4_validator_t valid_ipv4;

    namespace detail
    {

        /// @brief Dereferences a pointer-like value.
        ///
        /// Lets the set-membership validators accept either a container or a pointer
        /// to one without branching at every use.
        ///
        /// @param value The pointer to dereference.
        /// @return A reference to the pointed-at object.
        template <typename T>
            requires copyable_ptr<std::remove_reference_t<T>>
        auto smart_deref(T value) -> decltype(*value)
        {
            return *value;
        }

        /// @brief Passes a non-pointer value through unchanged.
        ///
        /// @param value The value to return.
        /// @return A reference to @p value.
        template <typename T>
            requires(!copyable_ptr<std::remove_reference_t<T>>)
        auto smart_deref(T &value) -> std::remove_reference_t<T> &
        {
            // NOLINTNEXTLINE
            return value;
        }

        /// @brief Renders a set as `{a,b,c}` for help output.
        ///
        /// @param set The set to render.
        /// @return The rendered set.
        template <typename T> auto generate_set(const T &set) -> std::string
        {
            using element_t = typename detail::element_type<T>::type;
            using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;

            std::string out(1, '{');
            out.append(detail::join(
                detail::smart_deref(set),
                [](const iteration_type_t &v) { return detail::pair_adaptor<element_t>::first(v); },
                ","));
            out.push_back('}');
            return out;
        }

        /// @brief Renders a map as `{k->v,k->v}` for help output.
        ///
        /// @param map The map to render.
        /// @param key_only Render only the keys, omitting the `->value` part.
        /// @return The rendered map.
        template <typename T> auto generate_map(const T &map, bool key_only = false) -> std::string
        {
            using element_t = typename detail::element_type<T>::type;
            using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;

            std::string out(1, '{');
            out.append(detail::join(
                detail::smart_deref(map),
                [key_only](const iteration_type_t &v) {
                    std::string res {detail::to_string(detail::pair_adaptor<element_t>::first(v))};
                    if (!key_only)
                    {
                        res.append("->");
                        res += detail::to_string(detail::pair_adaptor<element_t>::second(v));
                    }
                    return res;
                },
                ","));
            out.push_back('}');
            return out;
        }

        /// @brief Matches containers offering their own `find`.
        ///
        /// Used to pick the associative lookup over a linear scan. A pointer to a
        /// container does not satisfy this, so pointer-held sets take the scanning
        /// overload, which dereferences first.
        template <typename C, typename V>
        concept has_find = requires(C c, V v) { c.find(v); };

        /// @brief Finds a value in a container by scanning it.
        ///
        /// @param set The container to search.
        /// @param val The value to look for.
        /// @return Whether the value was found, and an iterator to it.
        template <typename T, typename V>
            requires(!has_find<T, V>)
        auto search(const T &set, const V &val) -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))>
        {
            using element_t = typename detail::element_type<T>::type;
            auto &setref = detail::smart_deref(set);
            auto it = std::ranges::find_if(setref, [&val](decltype(*std::begin(setref)) v) {
                return (detail::pair_adaptor<element_t>::first(v) == val);
            });
            return {(it != std::end(setref)), it};
        }

        /// @brief Finds a value in a container using its own `find`.
        ///
        /// @param set The container to search.
        /// @param val The value to look for.
        /// @return Whether the value was found, and an iterator to it.
        template <typename T, typename V>
            requires has_find<T, V>
        auto search(const T &set, const V &val) -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))>
        {
            auto &setref = detail::smart_deref(set);
            auto it = setref.find(val);
            return {(it != std::end(setref)), it};
        }

        /// @brief Finds a value, comparing through a filter when the direct lookup fails.
        ///
        /// Tries the plain lookup first, since it may be the associative one, and only
        /// falls back to scanning with the filter applied to each element.
        ///
        /// @param set The container to search.
        /// @param val The value to look for.
        /// @param filter_function Applied to each element before comparison.
        /// @return Whether the value was found, and an iterator to it.
        template <typename T, typename V>
        auto search(const T &set, const V &val, const std::function<V(V)> &filter_function)
            -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))>
        {
            using element_t = typename detail::element_type<T>::type;

            auto res = search(set, val);
            if ((res.first) || (!(filter_function)))
            {
                return res;
            }

            auto &setref = detail::smart_deref(set);
            auto it = std::ranges::find_if(setref, [&](decltype(*std::begin(setref)) v) {
                V a {detail::pair_adaptor<element_t>::first(v)};
                a = filter_function(a);
                return (a == val);
            });
            return {(it != std::end(setref)), it};
        }

    } // namespace detail

    /// @brief Requires the value to appear in a set.
    ///
    /// The set may be passed by value, as an initializer list, or as a pointer, in
    /// which case it is held and re-read on each check, so a set that changes after
    /// construction still validates correctly.
    ///
    /// One or more filter functions may be supplied; each is applied to both sides
    /// of the comparison, so `is_member_t{set, cli::ignore_case}` matches
    /// case-insensitively. On a filtered match the value is rewritten to the
    /// spelling stored in the set.
    class is_member_t : public validator_t
    {
        public:
            /// @brief A transformation applied to both sides of a comparison.
            using filter_fn_t = std::function<std::string(std::string)>;

            /// @brief Constructs from an initializer list.
            ///
            /// @param values The permitted values.
            /// @param args Any filter functions.
            template <typename T, typename... args_t>
            is_member_t(std::initializer_list<T> values, args_t &&...args)
                : is_member_t(std::vector<T>(values), std::forward<args_t>(args)...)
            {
            }

            /// @brief Constructs from a set, with no filtering.
            ///
            /// @param set The permitted values, by value or by pointer.
            template <typename T> explicit is_member_t(T &&set) : is_member_t(std::forward<T>(set), nullptr)
            {
            }

            /// @brief Constructs from a set and one filter function.
            ///
            /// @param set The permitted values, by value or by pointer.
            /// @param filter_function Applied to both sides of each comparison.
            template <typename T, typename F> explicit is_member_t(T set, F filter_function)
            {
                // element_t strips any pointer; item_t is the key type for a map and the
                // value type for anything else; local_item_t maps const char * to std::string.
                using element_t = typename detail::element_type<T>::type;
                using item_t = typename detail::pair_adaptor<element_t>::first_type;
                using local_item_t = typename is_member_type_t<item_t>::type;

                std::function<local_item_t(local_item_t)> filter_fn = filter_function;

                desc_function_ = [set] { return detail::generate_set(detail::smart_deref(set)); };

                // Captures the set by value, so a pointer-like set keeps its target alive.
                func_ = [set, filter_fn](std::string &input) {
                    using detail::lexical_cast;
                    local_item_t b;
                    if (!lexical_cast(input, b))
                    {
                        throw validation_error_t(input); // the option name is prepended later
                    }
                    if (filter_fn)
                    {
                        b = filter_fn(b);
                    }
                    auto res = detail::search(set, b, filter_fn);
                    if (res.first)
                    {
                        // Rewrite the input to the spelling held in the set.
                        if (filter_fn)
                        {
                            input = detail::value_string(detail::pair_adaptor<element_t>::first(*(res.second)));
                        }
                        return std::string {};
                    }
                    return input + " not in " + detail::generate_set(detail::smart_deref(set));
                };
            }

            /// @brief Constructs from a set and several filter functions, which nest.
            ///
            /// @param set The permitted values, by value or by pointer.
            /// @param filter_fn_1 The first filter, applied innermost.
            /// @param filter_fn_2 The second filter.
            /// @param other Any further filters.
            template <typename T, typename... args_t>
            is_member_t(T &&set, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, args_t &&...other)
                : is_member_t(
                      std::forward<T>(set),
                      [f1 = std::move(filter_fn_1), f2 = std::move(filter_fn_2)](std::string a) {
                          return f2(f1(std::move(a)));
                      },
                      other...)
            {
            }
    };

    /// @brief The default mapping type for the transformers.
    ///
    /// @tparam T The mapped-to type.
    template <typename T> using transform_pairs_t = std::vector<std::pair<std::string, T>>;

    /// @brief Rewrites a value through a lookup table, passing unknown values through.
    ///
    /// Use @ref cli::checked_transformer_t instead when an unrecognised value should
    /// be an error.
    class transformer_t : public validator_t
    {
        public:
            /// @brief A transformation applied to both sides of a comparison.
            using filter_fn_t = std::function<std::string(std::string)>;

            /// @brief Constructs from an initializer list of pairs.
            ///
            /// @param values The mapping.
            /// @param args Any filter functions.
            template <typename... args_t>
            transformer_t(std::initializer_list<std::pair<std::string, std::string>> values, args_t &&...args)
                : transformer_t(transform_pairs_t<std::string>(values), std::forward<args_t>(args)...)
            {
            }

            /// @brief Constructs from a mapping, with no filtering.
            ///
            /// @param mapping The mapping, by value or by pointer.
            template <typename T> explicit transformer_t(T &&mapping) : transformer_t(std::forward<T>(mapping), nullptr)
            {
            }

            /// @brief Constructs from a mapping and one filter function.
            ///
            /// @param mapping The mapping, by value or by pointer.
            /// @param filter_function Applied to both sides of each comparison.
            template <typename T, typename F> explicit transformer_t(T mapping, F filter_function)
            {
                static_assert(detail::pair_adaptor<typename detail::element_type<T>::type>::value,
                              "mapping must produce value pairs");

                using element_t = typename detail::element_type<T>::type;
                using item_t = typename detail::pair_adaptor<element_t>::first_type;
                using local_item_t = typename is_member_type_t<item_t>::type;

                std::function<local_item_t(local_item_t)> filter_fn = filter_function;

                desc_function_ = [mapping] { return detail::generate_map(detail::smart_deref(mapping)); };

                func_ = [mapping, filter_fn](std::string &input) {
                    using detail::lexical_cast;
                    local_item_t b;
                    if (!lexical_cast(input, b))
                    {
                        // Nothing in the mapping can match a value that will not convert.
                        return std::string();
                    }
                    if (filter_fn)
                    {
                        b = filter_fn(b);
                    }
                    auto res = detail::search(mapping, b, filter_fn);
                    if (res.first)
                    {
                        input = detail::value_string(detail::pair_adaptor<element_t>::second(*res.second));
                    }
                    return std::string {};
                };
            }

            /// @brief Constructs from a mapping and several filter functions, which nest.
            ///
            /// @param mapping The mapping, by value or by pointer.
            /// @param filter_fn_1 The first filter, applied innermost.
            /// @param filter_fn_2 The second filter.
            /// @param other Any further filters.
            template <typename T, typename... args_t>
            transformer_t(T &&mapping, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, args_t &&...other)
                : transformer_t(
                      std::forward<T>(mapping),
                      [f1 = std::move(filter_fn_1), f2 = std::move(filter_fn_2)](std::string a) {
                          return f2(f1(std::move(a)));
                      },
                      other...)
            {
            }
    };

    /// @brief Rewrites a value through a lookup table, rejecting unknown values.
    ///
    /// Behaves like @ref cli::transformer_t, except that a value matching neither a
    /// key nor an already-mapped output is an error.
    class checked_transformer_t : public validator_t
    {
        public:
            /// @brief A transformation applied to both sides of a comparison.
            using filter_fn_t = std::function<std::string(std::string)>;

            /// @brief Constructs from an initializer list of pairs.
            ///
            /// @param values The mapping.
            /// @param args Any filter functions.
            template <typename... args_t>
            checked_transformer_t(std::initializer_list<std::pair<std::string, std::string>> values, args_t &&...args)
                : checked_transformer_t(transform_pairs_t<std::string>(values), std::forward<args_t>(args)...)
            {
            }

            /// @brief Constructs from a mapping, with no filtering.
            ///
            /// @param mapping The mapping, by value or by pointer.
            template <typename T>
            explicit checked_transformer_t(T mapping) : checked_transformer_t(std::move(mapping), nullptr)
            {
            }

            /// @brief Constructs from a mapping and one filter function.
            ///
            /// @param mapping The mapping, by value or by pointer.
            /// @param filter_function Applied to both sides of each comparison.
            template <typename T, typename F> explicit checked_transformer_t(T mapping, F filter_function)
            {
                static_assert(detail::pair_adaptor<typename detail::element_type<T>::type>::value,
                              "mapping must produce value pairs");

                using element_t = typename detail::element_type<T>::type;
                using item_t = typename detail::pair_adaptor<element_t>::first_type;
                using local_item_t = typename is_member_type_t<item_t>::type;
                using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;

                std::function<local_item_t(local_item_t)> filter_fn = filter_function;

                auto tfunc = [mapping] {
                    std::string out("value in ");
                    out += detail::generate_map(detail::smart_deref(mapping)) + " OR {";
                    out += detail::join(
                        detail::smart_deref(mapping),
                        [](const iteration_type_t &v) {
                            return detail::value_string(detail::pair_adaptor<element_t>::second(v));
                        },
                        ",");
                    out.push_back('}');
                    return out;
                };

                desc_function_ = tfunc;

                func_ = [mapping, tfunc, filter_fn](std::string &input) {
                    using detail::lexical_cast;
                    local_item_t b;
                    const bool converted = lexical_cast(input, b);
                    if (converted)
                    {
                        if (filter_fn)
                        {
                            b = filter_fn(b);
                        }
                        auto res = detail::search(mapping, b, filter_fn);
                        if (res.first)
                        {
                            input = detail::value_string(detail::pair_adaptor<element_t>::second(*res.second));
                            return std::string {};
                        }
                    }

                    // Accept a value that is already one of the mapped outputs, so that
                    // transforming twice is harmless.
                    for (const auto &v : detail::smart_deref(mapping))
                    {
                        const auto output_string = detail::value_string(detail::pair_adaptor<element_t>::second(v));
                        if (output_string == input)
                        {
                            return std::string();
                        }
                    }

                    return "Check " + input + " " + tfunc() + " FAILED";
                };
            }

            /// @brief Constructs from a mapping and several filter functions, which nest.
            ///
            /// @param mapping The mapping, by value or by pointer.
            /// @param filter_fn_1 The first filter, applied innermost.
            /// @param filter_fn_2 The second filter.
            /// @param other Any further filters.
            template <typename T, typename... args_t>
            checked_transformer_t(T &&mapping, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, args_t &&...other)
                : checked_transformer_t(
                      std::forward<T>(mapping),
                      [f1 = std::move(filter_fn_1), f2 = std::move(filter_fn_2)](std::string a) {
                          return f2(f1(std::move(a)));
                      },
                      other...)
            {
            }
    };

    /// @brief Filter that makes a comparison case-insensitive.
    ///
    /// Pass to @ref cli::is_member_t or one of the transformers.
    ///
    /// @param item The value to fold.
    /// @return The lowercased value.
    auto ignore_case(std::string item) -> std::string
    {
        return detail::to_lower(item);
    }

    /// @brief Filter that makes a comparison ignore underscores.
    ///
    /// @param item The value to fold.
    /// @return The value with underscores removed.
    auto ignore_underscore(std::string item) -> std::string
    {
        return detail::remove_underscore(item);
    }

    /// @brief Filter that makes a comparison ignore spaces and tabs.
    ///
    /// @param item The value to fold.
    /// @return The value with spaces and tabs removed.
    auto ignore_space(std::string item) -> std::string
    {
        std::erase(item, ' ');
        std::erase(item, '\t');
        return item;
    }

    /// @brief Multiplies a value by a unit factor drawn from a mapping.
    ///
    /// Given a mapping of `{"b" -> 1, "kb" -> 1024, "mb" -> 1024*1024}`, inputs such
    /// as `"100"`, `"12kb"`, and `"100 MB"` become `100`, `12288`, and `104857600`.
    ///
    /// The result type follows the mapping's value type, so to accept fractional
    /// inputs such as `"0.42 s"` the mapping must map to `float` or `double`.
    class as_number_with_unit_t : public validator_t
    {
        public:
            /// @brief Flags controlling how units are matched.
            ///
            /// A bitmask. Combine with `|` and test with @ref has_flag.
            ///
            /// @note `default_mode` rather than `default`, which is a keyword. Its
            /// value is `case_insensitive | unit_optional`, spelled numerically
            /// because the enumerators are not yet usable with `|` at this point.
            enum class options_t : std::uint8_t
            {
                case_sensitive = 0,   ///< Units must match exactly.
                case_insensitive = 1, ///< Units match without regard to case.
                unit_optional = 0,    ///< A value with no unit is accepted.
                unit_required = 2,    ///< A value with no unit is an error.
                default_mode = 1      ///< `case_insensitive | unit_optional`.
            };

            /// @brief Combines two option flags.
            ///
            /// @param a The left operand.
            /// @param b The right operand.
            /// @return The combined flags.
            friend constexpr auto operator|(options_t a, options_t b) -> options_t
            {
                return static_cast<options_t>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
            }

            /// @brief Intersects two option flags.
            ///
            /// @param a The left operand.
            /// @param b The right operand.
            /// @return The common flags.
            friend constexpr auto operator&(options_t a, options_t b) -> options_t
            {
                return static_cast<options_t>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
            }

            /// @brief Adds flags in place.
            ///
            /// @param[in,out] a The flags to modify.
            /// @param[in] b The flags to add.
            /// @return A reference to @p a.
            friend constexpr auto operator|=(options_t &a, options_t b) -> options_t &
            {
                a = a | b;
                return a;
            }

            /// @brief Removes flags in place.
            ///
            /// @param[in,out] a The flags to modify.
            /// @param[in] b The mask to apply.
            /// @return A reference to @p a.
            friend constexpr auto operator&=(options_t &a, options_t b) -> options_t &
            {
                a = a & b;
                return a;
            }

            /// @brief Tests whether a flag is set.
            ///
            /// Replaces the implicit conversion to `bool` that the unscoped enum
            /// used to allow.
            ///
            /// @param value The flags to inspect.
            /// @param flag The flag to look for.
            /// @return `true` if @p flag is set in @p value.
            friend constexpr auto has_flag(options_t value, options_t flag) -> bool
            {
                return static_cast<std::uint8_t>(value & flag) != 0;
            }

            /// @brief Constructs the validator from a unit mapping.
            ///
            /// @tparam number_t The numeric type the result is produced in.
            /// @param mapping Unit names mapped to their multipliers.
            /// @param opts How units are matched.
            /// @param unit_name The label used for the unit in help output.
            /// @throws cli::validation_error_t At parse time, if the value is empty, the
            /// unit is missing or unrecognised, or the multiplication would overflow.
            template <typename number_t>
            explicit as_number_with_unit_t(std::map<std::string, number_t> mapping,
                                           options_t opts = options_t::default_mode,
                                           const std::string &unit_name = "UNIT")
            {
                description(generate_description<number_t>(unit_name, opts));
                validate_mapping(mapping, opts);

                func_ = [mapping, opts](std::string &input) -> std::string {
                    number_t num {};

                    detail::rtrim(input);
                    if (input.empty())
                    {
                        throw validation_error_t("Input is empty");
                    }

                    // Split the trailing alphabetic run off as the unit.
                    auto unit_begin = input.end();
                    while (unit_begin > input.begin() && std::isalpha(*(unit_begin - 1), std::locale()))
                    {
                        --unit_begin;
                    }

                    std::string unit {unit_begin, input.end()};
                    input.resize(static_cast<std::size_t>(std::distance(input.begin(), unit_begin)));
                    detail::trim(input);

                    if (has_flag(opts, options_t::unit_required) && unit.empty())
                    {
                        throw validation_error_t("Missing mandatory unit");
                    }
                    if (has_flag(opts, options_t::case_insensitive))
                    {
                        unit = detail::to_lower(unit);
                    }
                    if (unit.empty())
                    {
                        using detail::lexical_cast;
                        if (!lexical_cast(input, num))
                        {
                            throw validation_error_t(std::string("Value ") + input + " could not be converted to " +
                                                     detail::type_name<number_t>());
                        }
                        // Nothing to rewrite when no unit was given.
                        return {};
                    }

                    const auto it = mapping.find(unit);
                    if (it == mapping.end())
                    {
                        throw validation_error_t(
                            unit + " unit not recognized. Allowed values: " + detail::generate_map(mapping, true));
                    }

                    if (!input.empty())
                    {
                        using detail::lexical_cast;
                        const bool converted = lexical_cast(input, num);
                        if (!converted)
                        {
                            throw validation_error_t(std::string("Value ") + input + " could not be converted to " +
                                                     detail::type_name<number_t>());
                        }
                        if (!detail::checked_multiply(num, it->second))
                        {
                            throw validation_error_t(detail::to_string(num) + " multiplied by " + unit +
                                                     " factor would cause number overflow. Use smaller value.");
                        }
                    }
                    else
                    {
                        num = static_cast<number_t>(it->second);
                    }

                    input = detail::to_string(num);
                    return {};
                };
            }

        private:
            /// @brief Checks that every unit is usable, and folds case if asked.
            ///
            /// @param[in,out] mapping The unit mapping; lowercased in place under
            /// @ref options_t::case_insensitive.
            /// @param[in] opts How units are matched.
            /// @throws cli::validation_error_t If a unit is empty, contains anything
            /// other than letters, or collides with another once lowercased.
            template <typename number_t>
            static auto validate_mapping(std::map<std::string, number_t> &mapping, options_t opts) -> void
            {
                for (auto &kv : mapping)
                {
                    if (kv.first.empty())
                    {
                        throw validation_error_t("Unit must not be empty.");
                    }
                    if (!detail::isalpha(kv.first))
                    {
                        throw validation_error_t("Unit must contain only letters.");
                    }
                }

                if (has_flag(opts, options_t::case_insensitive))
                {
                    std::map<std::string, number_t> lower_mapping;
                    for (auto &kv : mapping)
                    {
                        auto s = detail::to_lower(kv.first);
                        if (lower_mapping.contains(s))
                        {
                            throw validation_error_t(
                                std::string("Several matching lowercase unit representations are found: ") + s);
                        }
                        lower_mapping[std::move(s)] = kv.second;
                    }
                    mapping = std::move(lower_mapping);
                }
            }

            /// @brief Builds the description, such as `FLOAT [UNIT]`.
            ///
            /// The unit is bracketed when it is optional.
            ///
            /// @param name The label used for the unit.
            /// @param opts How units are matched.
            /// @return The rendered description.
            template <typename number_t>
            static auto generate_description(const std::string &name, options_t opts) -> std::string
            {
                std::ostringstream out;
                out << detail::type_name<number_t>() << ' ';
                if (has_flag(opts, options_t::unit_required))
                {
                    out << name;
                }
                else
                {
                    out << '[' << name << ']';
                }
                return out.str();
            }
    };

    /// @brief Converts a human-readable size to a byte count.
    ///
    /// `"100"` gives 100, `"10Kb"` gives 10240, `"2 MB"` gives 2097152, and units are
    /// recognised up to exbibyte. The `*i` and `*ib` spellings always mean powers of
    /// 1024; whether the plain spellings do is chosen at construction.
    class as_size_value_t : public as_number_with_unit_t
    {
        public:
            /// @brief The type sizes are produced in.
            using result_t = std::uint64_t;

            /// @brief Constructs the validator.
            ///
            /// @param kb_is_1000 When `true`, `kb` and `k` mean 1000 while `kib` and
            /// `ki` mean 1024, and likewise for the larger units. When `false`, every
            /// spelling means a power of 1024 — formally wrong, but the more common
            /// reading. See https://en.wikipedia.org/wiki/Binary_prefix.
            explicit as_size_value_t(bool kb_is_1000);

        private:
            /// @brief Builds the unit-to-factor mapping.
            ///
            /// @param kb_is_1000 Whether the plain spellings mean powers of 1000.
            /// @return The mapping.
            static auto init_mapping(bool kb_is_1000) -> std::map<std::string, result_t>;

            /// @brief Returns the unit mapping, building it once per variant.
            ///
            /// @param kb_is_1000 Whether the plain spellings mean powers of 1000.
            /// @return A reference to the cached mapping.
            static auto get_mapping(bool kb_is_1000) -> const std::map<std::string, result_t> &;
    };

    namespace detail
    {

        /// @brief A filesystem permission to check for.
        enum class permission_t : std::uint8_t
        {
            none = 0,  ///< No permission check.
            read = 1,  ///< Readable.
            write = 2, ///< Writable.
            exec = 4   ///< Executable.
        };

        /// @brief Requires the path to exist and carry a given permission.
        class permission_validator_t : public validator_t
        {
            public:
                /// @brief Constructs the validator.
                ///
                /// @param permission The permission to require.
                explicit permission_validator_t(permission_t permission);
        };

    } // namespace detail

    /// @brief Requires the path to name a file within a size range.
    class file_size_validator_t : public validator_t
    {
        public:
            /// @brief Constructs the validator.
            ///
            /// @param min_size The smallest permitted size, in bytes.
            /// @param max_size The largest permitted size, in bytes; `0` means unbounded.
            explicit file_size_validator_t(std::uint64_t min_size, std::uint64_t max_size = 0);
    };

    /// @brief Requires the path to exist and be readable.
    const detail::permission_validator_t read_permissions(detail::permission_t::read);

    /// @brief Requires the path to exist and be writable.
    const detail::permission_validator_t write_permissions(detail::permission_t::write);

    /// @brief Requires the path to exist and be executable.
    const detail::permission_validator_t exec_permissions(detail::permission_t::exec);

    /// @brief Requires the path to name a file that is not empty.
    const file_size_validator_t non_empty_file(1, 0);

    // =============================================
    // Implementation
    // =============================================

    namespace detail
    {

        ipv4_validator_t::ipv4_validator_t() : validator_t("IPV4")
        {
            func_ = [](std::string &ip_addr) {
                const auto cdot = std::ranges::count(ip_addr, '.');
                if (cdot != 3)
                {
                    return std::string("Invalid IPV4 address: must have 3 separators");
                }
                const auto result = detail::split(ip_addr, '.');
                if (result.size() != 4)
                {
                    return std::string("Invalid IPV4 address: must have four parts (") + ip_addr + ')';
                }
                int num = 0;
                for (const auto &var : result)
                {
                    using detail::lexical_cast;
                    if (!lexical_cast(var, num))
                    {
                        return std::string("Failed parsing number (") + var + ')';
                    }
                    if (num < 0 || num > 255)
                    {
                        return std::string("Each IP number must be between 0 and 255 ") + var;
                    }
                }
                return std::string {};
            };
        }

    } // namespace detail

    as_size_value_t::as_size_value_t(bool kb_is_1000) : as_number_with_unit_t(get_mapping(kb_is_1000))
    {
        if (kb_is_1000)
        {
            description("SIZE [b, kb(=1000b), kib(=1024b), ...]");
        }
        else
        {
            description("SIZE [b, kb(=1024b), ...]");
        }
    }

    auto as_size_value_t::init_mapping(bool kb_is_1000) -> std::map<std::string, result_t>
    {
        std::map<std::string, result_t> m;
        const result_t k_factor = kb_is_1000 ? 1000 : 1024;
        const result_t ki_factor = 1024;
        result_t k = 1;
        result_t ki = 1;

        m["b"] = 1;
        for (const std::string p : {"k", "m", "g", "t", "p", "e"})
        {
            k *= k_factor;
            ki *= ki_factor;
            m[p] = k;
            m[p + "b"] = k;
            m[p + "i"] = ki;
            m[p + "ib"] = ki;
        }
        return m;
    }

    auto as_size_value_t::get_mapping(bool kb_is_1000) -> const std::map<std::string, result_t> &
    {
        if (kb_is_1000)
        {
            static const auto m = init_mapping(true);
            return m;
        }
        static const auto m = init_mapping(false);
        return m;
    }

    namespace detail
    {

        permission_validator_t::permission_validator_t(permission_t permission)
        {
            std::filesystem::perms permission_code = std::filesystem::perms::none;
            std::string permission_name;

            switch (permission)
            {
            case permission_t::read:
                permission_code = std::filesystem::perms::owner_read | std::filesystem::perms::group_read |
                                  std::filesystem::perms::others_read;
                permission_name = "read";
                break;
            case permission_t::write:
                permission_code = std::filesystem::perms::owner_write | std::filesystem::perms::group_write |
                                  std::filesystem::perms::others_write;
                permission_name = "write";
                break;
            case permission_t::exec:
                permission_code = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                  std::filesystem::perms::others_exec;
                permission_name = "exec";
                break;
            case permission_t::none:
            default:
                permission_code = std::filesystem::perms::none;
                break;
            }

            func_ = [permission_code](std::string &path) {
                std::error_code ec;
                const auto p = std::filesystem::path(path);
                if (!std::filesystem::exists(p, ec))
                {
                    return std::string("Path does not exist: ") + path;
                }
                if (ec)
                {
                    return std::string("Error checking path: ") + ec.message(); // LCOV_EXCL_LINE
                }
                if (permission_code == std::filesystem::perms::none)
                {
                    return std::string {};
                }
                const auto perms = std::filesystem::status(p, ec).permissions();
                if (ec)
                {
                    return std::string("Error checking path status: ") + ec.message(); // LCOV_EXCL_LINE
                }
                if ((perms & permission_code) == std::filesystem::perms::none)
                {
                    return std::string("Path does not have required permissions: ") + path;
                }
                return std::string {};
            };

            description("Path with " + permission_name + " permission");
        }

    } // namespace detail

    file_size_validator_t::file_size_validator_t(std::uint64_t min_size, std::uint64_t max_size)
    {
        std::string desc;
        if (max_size == 0)
        {
            desc = "File size at least " + std::to_string(min_size) + " bytes";
        }
        else
        {
            desc = "File size between " + std::to_string(min_size) + " and " + std::to_string(max_size) + " bytes";
        }
        description(desc);

        func_ = [min_size, max_size](std::string &path) {
            std::error_code ec;
            const auto p = std::filesystem::path(path);
            if (!std::filesystem::exists(p, ec))
            {
                return std::string("File does not exist: ") + path;
            }
            if (ec)
            {
                return std::string("Error checking file: ") + ec.message(); // LCOV_EXCL_LINE
            }
            const auto size = std::filesystem::file_size(p, ec);
            if (ec)
            {
                return std::string("Error getting file size: ") + ec.message(); // LCOV_EXCL_LINE
            }
            if (size < min_size)
            {
                return std::string("File size ") + std::to_string(size) + " bytes is less than minimum " +
                       std::to_string(min_size) + " bytes";
            }
            if (max_size > 0 && size > max_size)
            {
                return std::string("File size ") + std::to_string(size) + " bytes exceeds maximum " +
                       std::to_string(max_size) + " bytes";
            }
            return std::string {};
        };
    }

} // namespace cli
