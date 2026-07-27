/// @file
/// @brief Type classification and string conversion.
///
/// This is the engine that lets `add_option` accept an arbitrary variable and
/// work out how to fill it from the command line. It answers three questions
/// about a type:
///
/// - **What is it?** @ref cli::detail::object_category_t and
///   @ref cli::detail::classify_object sort every type into one of eighteen
///   buckets: integral, container, tuple, wrapper, and so on.
/// - **How many values does it need?** @ref cli::detail::type_count and its
///   relatives compute how many command-line tokens a type consumes.
/// - **How do I fill it?** @ref cli::detail::lexical_cast converts one token,
///   @ref cli::detail::lexical_conversion converts a whole list.
///
/// Dispatch is by concept. Each category has a matching concept, so an overload
/// reads as `template <char_value_like T> auto lexical_cast(...)` rather than
/// carrying its condition in a defaulted template parameter.

module;
// C standard library macros must be included in the global module fragment
#include <cerrno>

export module cli11:type_tools;

import std;
import :encoding;
import :string_tools;

export namespace cli
{

    /// @brief Matches exactly `bool`, excluding types merely convertible to it.
    template <typename T>
    concept bool_like = std::same_as<std::remove_cv_t<T>, bool>;

    /// @brief Matches `std::shared_ptr`, with or without top-level const.
    template <typename T>
    concept shared_ptr_like = requires {
        typename std::remove_cv_t<T>::element_type;
    } && std::same_as<std::remove_cv_t<T>, std::shared_ptr<typename std::remove_cv_t<T>::element_type>>;

    /// @brief Matches pointer-like types that can be copied freely.
    template <typename T>
    concept copyable_ptr = shared_ptr_like<T> || std::is_pointer_v<T>;

    /// @brief Maps a type to the one used to store it in a member set.
    ///
    /// The identity mapping, except that `const char *` becomes `std::string` so
    /// that string literals in an `is_member` set are stored by value.
    ///
    /// @tparam T The type to map.
    template <typename T> struct is_member_type_t
    {
            /// @brief The storage type.
            using type = T;
    };

    /// @brief Stores string literals as `std::string`.
    template <> struct is_member_type_t<const char *>
    {
            /// @brief The storage type.
            using type = std::string;
    };

    namespace adl_detail
    {

        /// @brief Matches types with a `lexical_cast` overload findable by ADL.
        ///
        /// @tparam T The type being converted into.
        /// @tparam S The source string type.
        template <typename T, typename S = std::string>
        concept lexical_castable = requires(const S &src, T &dst) { lexical_cast(src, dst); };

    } // namespace adl_detail

    namespace detail
    {

        /// @brief The type a pointer-like type points at, or the type itself.
        ///
        /// @tparam T The type to inspect.
        template <typename T> struct element_type
        {
                /// @brief The pointed-at type, or @p T when it is not a pointer.
                using type = T;
        };

        /// @brief Unwraps pointer-like types.
        template <copyable_ptr T> struct element_type<T>
        {
                /// @brief The pointed-at type.
                using type = typename std::pointer_traits<T>::element_type;
        };

        /// @brief The `value_type` of a container, seen through any pointer.
        ///
        /// @tparam T The container, or a pointer to one.
        template <typename T> struct element_value_type
        {
                /// @brief The container's element type.
                using type = typename element_type<T>::type::value_type;
        };

        /// @brief Matches containers whose elements are pair-like.
        ///
        /// @tparam T The container to inspect.
        template <typename T>
        concept pair_valued = requires {
            typename T::value_type::first_type;
            typename T::value_type::second_type;
        };

        /// @brief Uniform access to the halves of a container's element.
        ///
        /// For a map-like container the halves are the key and the value. For any
        /// other container both halves are the element itself, so that code
        /// handling `is_member` sets does not need to branch on container shape.
        ///
        /// @tparam T The container to adapt.
        template <typename T> struct pair_adaptor : std::false_type
        {
                /// @brief The container's element type.
                using value_type = typename T::value_type;

                /// @brief The type of the first half.
                using first_type = std::remove_const_t<value_type>;

                /// @brief The type of the second half.
                using second_type = std::remove_const_t<value_type>;

                /// @brief Returns the first half of an element.
                ///
                /// @param pair_value The element.
                /// @return The element itself.
                template <typename Q> static auto first(Q &&pair_value) -> decltype(std::forward<Q>(pair_value))
                {
                    return std::forward<Q>(pair_value);
                }

                /// @brief Returns the second half of an element.
                ///
                /// @param pair_value The element.
                /// @return The element itself.
                template <typename Q> static auto second(Q &&pair_value) -> decltype(std::forward<Q>(pair_value))
                {
                    return std::forward<Q>(pair_value);
                }
        };

        /// @brief Adapts containers whose elements really are pairs.
        template <pair_valued T> struct pair_adaptor<T> : std::true_type
        {
                /// @brief The container's element type.
                using value_type = typename T::value_type;

                /// @brief The key type.
                using first_type = std::remove_const_t<typename value_type::first_type>;

                /// @brief The mapped type.
                using second_type = std::remove_const_t<typename value_type::second_type>;

                /// @brief Returns the key half of an element.
                ///
                /// @param pair_value The element.
                /// @return The key.
                template <typename Q>
                static auto first(Q &&pair_value) -> decltype(std::get<0>(std::forward<Q>(pair_value)))
                {
                    return std::get<0>(std::forward<Q>(pair_value));
                }

                /// @brief Returns the mapped half of an element.
                ///
                /// @param pair_value The element.
                /// @return The mapped value.
                template <typename Q>
                static auto second(Q &&pair_value) -> decltype(std::get<1>(std::forward<Q>(pair_value)))
                {
                    return std::get<1>(std::forward<Q>(pair_value));
                }
        };

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

        /// @brief Matches types brace-constructible from @p C without narrowing.
        ///
        /// Brace initialisation is deliberate: it rejects narrowing conversions, so
        /// a type taking `double` is not treated as taking `int`. Move-assignability
        /// is required because the conversion machinery assigns the result.
        ///
        /// @tparam T The type to construct.
        /// @tparam C The type to construct it from.
        template <typename T, typename C>
        concept direct_constructible =
            std::is_constructible_v<T, C> && std::is_move_assignable_v<T> && requires { T {std::declval<C>()}; };

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

        /// @brief Matches types that can be written to an output stream.
        template <typename T, typename S = std::ostringstream>
        concept ostreamable = requires(S &s, const T &v) { s << v; };

        /// @brief Matches types that can be read from an input stream.
        template <typename T, typename S = std::istringstream>
        concept istreamable = requires(S &s, T &v) { s >> v; };

        /// @brief Matches complex-number-like types, by their accessors.
        template <typename T>
        concept complex_like = requires(T v) {
            v.real();
            v.imag();
        };

        /// @brief Reads a value from a string using its stream extraction operator.
        ///
        /// @param[in] istring The text to read.
        /// @param[out] obj The value to fill.
        /// @return `true` if the whole string was consumed without error.
        template <istreamable T> auto from_stream(const std::string &istring, T &obj) -> bool
        {
            std::istringstream is;
            is.str(istring);
            is >> obj;
            return !is.fail() && !is.rdbuf()->in_avail();
        }

        /// @brief Fallback for types with no stream extraction operator.
        ///
        /// @return Always `false`.
        template <typename T>
            requires(!istreamable<T>)
        auto from_stream(const std::string & /*istring*/, T & /*obj*/) -> bool
        {
            return false;
        }

        /// @brief Matches containers that can be cleared and inserted into.
        ///
        /// String types are excluded even though they satisfy the operations, since
        /// they are handled as scalars rather than as sequences of characters.
        template <typename T>
        concept mutable_container =
            requires(T c) {
                typename T::value_type;
                c.end();
                c.clear();
                c.insert(c.end(), std::declval<const typename T::value_type &>());
            } && !std::is_constructible_v<T, std::string> && !std::is_constructible_v<T, std::wstring>;

        /// @brief Matches anything that can be iterated over.
        template <typename T>
        concept readable_container = requires(T c) {
            c.begin();
            c.end();
        };

        /// @brief Matches types with a nested `value_type`, such as `std::optional`.
        template <typename T>
        concept wrapper_like = requires { typename T::value_type; };

        /// @brief Matches types usable with `std::tuple_size`, excluding complex numbers.
        template <typename T>
        concept tuple_like = !complex_like<T> && requires { std::tuple_size<std::decay_t<T>>::value; };

        /// @brief How many values a type consumes, ignoring nesting.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct type_count_base
        {
                /// @brief The value count.
                static constexpr int value {0};
        };

        /// @brief Scalars consume one value.
        template <typename T>
            requires(!tuple_like<T> && !mutable_container<T> && !std::is_void_v<T>)
        struct type_count_base<T>
        {
                /// @brief The value count.
                static constexpr int value {1};
        };

        /// @brief Tuples consume one value per element.
        template <typename T>
            requires(tuple_like<T> && !mutable_container<T>)
        struct type_count_base<T>
        {
                /// @brief The value count.
                static constexpr int value {static_cast<int>(std::tuple_size<std::decay_t<T>>::value)};
        };

        /// @brief Containers report the count of their element type.
        template <mutable_container T> struct type_count_base<T>
        {
                /// @brief The value count.
                static constexpr int value {type_count_base<typename T::value_type>::value};
        };

        /// @brief Convenience accessor for @ref type_count_base.
        template <typename T> constexpr int type_count_base_v = type_count_base<T>::value;

        /// @brief Matches types implicitly convertible to `std::string`.
        template <typename T>
        concept string_convertible = std::is_convertible_v<T, std::string>;

        /// @brief Matches types a `std::string` can be constructed from.
        template <typename T>
        concept string_buildable = std::is_constructible_v<std::string, T>;

        /// @brief Matches types with no string conversion and no stream insertion.
        ///
        /// These need structural handling: tuple, container, or nothing at all.
        template <typename T>
        concept plain_object = !string_convertible<T> && !string_buildable<T> && !ostreamable<T>;

        /// @brief Renders a value already convertible to a string.
        ///
        /// @param value The value to render.
        /// @return The value, forwarded unchanged.
        template <string_convertible T> auto to_string(T &&value) -> decltype(std::forward<T>(value))
        {
            return std::forward<T>(value);
        }

        /// @brief Renders a value a string can be built from.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename T>
            requires(string_buildable<T> && !string_convertible<T>)
        auto to_string(T &&value) -> std::string
        {
            return std::string(value);
        }

        /// @brief Renders a value through its stream insertion operator.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename T>
            requires(!string_convertible<T> && !string_buildable<T> && ostreamable<T>)
        auto to_string(T &&value) -> std::string
        {
            std::stringstream stream;
            stream << value;
            return stream.str();
        }

        /// @brief Renders a single-element tuple as its one element.
        ///
        /// @param value The tuple to render.
        /// @return The rendered element.
        template <typename T>
            requires(plain_object<T> && tuple_like<T> && type_count_base_v<T> == 1)
        auto to_string(T &&value) -> std::string;

        /// @brief Renders a multi-element tuple as a bracketed list.
        ///
        /// @param value The tuple to render.
        /// @return The rendered tuple.
        template <typename T>
            requires(plain_object<T> && tuple_like<T> && type_count_base_v<T> >= 2)
        auto to_string(T &&value) -> std::string;

        /// @brief Renders anything with no usable representation as an empty string.
        ///
        /// @return An empty string.
        template <typename T>
            requires(plain_object<T> && !readable_container<std::remove_const_t<T>> && !tuple_like<T>)
        auto to_string(T &&) -> std::string
        {
            return {};
        }

        /// @brief Renders a container as a bracketed, comma-separated list.
        ///
        /// @param variable The container to render.
        /// @return The rendered container, or `"{}"` if it is empty.
        template <typename T>
            requires(plain_object<T> && readable_container<T> && !tuple_like<T>)
        auto to_string(T &&variable) -> std::string
        {
            auto cval = variable.begin();
            auto end = variable.end();
            if (cval == end)
            {
                return {"{}"};
            }
            std::vector<std::string> defaults;
            while (cval != end)
            {
                defaults.emplace_back(to_string(*cval));
                ++cval;
            }
            return {"[" + join(defaults) + "]"};
        }

        /// @brief Terminates the tuple rendering recursion.
        ///
        /// @return An empty string.
        template <typename T, std::size_t I>
            requires(I == type_count_base_v<T>)
        auto tuple_value_string(T && /*value*/) -> std::string;

        /// @brief Renders tuple elements from index @p I onward.
        ///
        /// @param value The tuple to render.
        /// @return The rendered elements, comma-separated.
        template <typename T, std::size_t I>
            requires(I < type_count_base_v<T>)
        auto tuple_value_string(T &&value) -> std::string;

        template <typename T>
            requires(plain_object<T> && tuple_like<T> && type_count_base_v<T> == 1)
        auto to_string(T &&value) -> std::string
        {
            return to_string(std::get<0>(value));
        }

        template <typename T>
            requires(plain_object<T> && tuple_like<T> && type_count_base_v<T> >= 2)
        auto to_string(T &&value) -> std::string
        {
            auto tname = std::string(1, '[') + tuple_value_string<T, 0>(value);
            tname.push_back(']');
            return tname;
        }

        template <typename T, std::size_t I>
            requires(I == type_count_base_v<T>)
        auto tuple_value_string(T && /*value*/) -> std::string
        {
            return std::string {};
        }

        template <typename T, std::size_t I>
            requires(I < type_count_base_v<T>)
        auto tuple_value_string(T &&value) -> std::string
        {
            auto str = std::string {to_string(std::get<I>(value))} + ',' + tuple_value_string<T, I + 1>(value);
            if (str.back() == ',')
            {
                str.pop_back();
            }
            return str;
        }

        /// @brief Renders a value only when two types agree.
        ///
        /// Used where a default is only meaningful if the assigned and converted
        /// types are the same.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename lhs_t, typename rhs_t, typename T>
            requires std::same_as<lhs_t, rhs_t>
        auto checked_to_string(T &&value) -> decltype(to_string(std::forward<T>(value)))
        {
            return to_string(std::forward<T>(value));
        }

        /// @brief Renders nothing when the two types differ.
        ///
        /// @return An empty string.
        template <typename lhs_t, typename rhs_t, typename T>
            requires(!std::same_as<lhs_t, rhs_t>)
        auto checked_to_string(T &&) -> std::string
        {
            return std::string {};
        }

        /// @brief Renders an arithmetic value.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename T>
            requires std::is_arithmetic_v<T>
        auto value_string(const T &value) -> std::string
        {
            return std::to_string(value);
        }

        /// @brief Renders an enumerator as its underlying integer.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename T>
            requires std::is_enum_v<T>
        auto value_string(const T &value) -> std::string
        {
            return std::to_string(static_cast<std::underlying_type_t<T>>(value));
        }

        /// @brief Renders any other value through @ref to_string.
        ///
        /// @param value The value to render.
        /// @return The rendered value.
        template <typename T>
            requires(!std::is_enum_v<T> && !std::is_arithmetic_v<T>)
        auto value_string(const T &value) -> decltype(to_string(value))
        {
            return to_string(value);
        }

        /// @brief The type inside a wrapper, or a supplied default.
        ///
        /// @tparam T The type to unwrap.
        /// @tparam def_t The type to report when @p T is not a wrapper.
        template <typename T, typename def_t> struct wrapped_type
        {
                /// @brief The unwrapped type.
                using type = def_t;
        };

        /// @brief Unwraps types with a nested `value_type`.
        template <wrapper_like T, typename def_t> struct wrapped_type<T, def_t>
        {
                /// @brief The unwrapped type.
                using type = typename T::value_type;
        };

        template <typename T> struct subtype_count;
        template <typename T> struct subtype_count_min;

        /// @brief How many values a type consumes, counting nested elements.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct type_count
        {
                /// @brief The value count.
                static constexpr int value {0};
        };

        /// @brief Plain scalars consume one value.
        template <typename T>
            requires(!wrapper_like<T> && !tuple_like<T> && !complex_like<T> && !std::is_void_v<T>)
        struct type_count<T>
        {
                /// @brief The value count.
                static constexpr int value {1};
        };

        /// @brief Complex numbers consume two values.
        template <complex_like T> struct type_count<T>
        {
                /// @brief The value count.
                static constexpr int value {2};
        };

        /// @brief Containers report the nested count of their element type.
        template <mutable_container T> struct type_count<T>
        {
                /// @brief The value count.
                static constexpr int value {subtype_count<typename T::value_type>::value};
        };

        /// @brief Wrappers report the count of the type they hold.
        template <typename T>
            requires(wrapper_like<T> && !complex_like<T> && !tuple_like<T> && !mutable_container<T>)
        struct type_count<T>
        {
                /// @brief The value count.
                static constexpr int value {type_count<typename T::value_type>::value};
        };

        /// @brief Terminates the tuple size recursion.
        ///
        /// @return Zero.
        template <typename T, std::size_t I>
            requires(I == type_count_base_v<T>)
        constexpr auto tuple_type_size() -> int
        {
            return 0;
        }

        /// @brief Sums the value counts of tuple elements from index @p I onward.
        ///
        /// @return The summed count.
        template <typename T, std::size_t I>
            requires(I < type_count_base_v<T>)
        constexpr auto tuple_type_size() -> int
        {
            return subtype_count<std::tuple_element_t<I, T>>::value + tuple_type_size<T, I + 1>();
        }

        /// @brief Tuples sum the counts of their elements.
        template <typename T>
            requires(tuple_like<T> && !complex_like<T>)
        struct type_count<T>
        {
                /// @brief The value count.
                static constexpr int value {tuple_type_size<T, 0>()};
        };

        /// @brief Convenience accessor for @ref type_count.
        template <typename T> constexpr int type_count_v = type_count<T>::value;

        /// @brief Value count of a nested type, treating containers as unbounded.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct subtype_count
        {
                /// @brief The value count.
                static constexpr int value {mutable_container<T> ? expected_max_vector_size : type_count<T>::value};
        };

        /// @brief The minimum number of values a type will accept.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct type_count_min
        {
                /// @brief The minimum value count.
                static constexpr int value {0};
        };

        /// @brief Plain scalars require their full count.
        template <typename T>
            requires(!mutable_container<T> && !tuple_like<T> && !wrapper_like<T> && !complex_like<T> &&
                     !std::is_void_v<T>)
        struct type_count_min<T>
        {
                /// @brief The minimum value count.
                static constexpr int value {type_count<T>::value};
        };

        /// @brief Complex numbers accept a single value, the real part alone.
        template <complex_like T> struct type_count_min<T>
        {
                /// @brief The minimum value count.
                static constexpr int value {1};
        };

        /// @brief Wrappers report the minimum of the type they hold.
        template <typename T>
            requires(wrapper_like<T> && !complex_like<T> && !tuple_like<T>)
        struct type_count_min<T>
        {
                /// @brief The minimum value count.
                static constexpr int value {subtype_count_min<typename T::value_type>::value};
        };

        /// @brief Terminates the minimum tuple size recursion.
        ///
        /// @return Zero.
        template <typename T, std::size_t I>
            requires(I == type_count_base_v<T>)
        constexpr auto tuple_type_size_min() -> int
        {
            return 0;
        }

        /// @brief Sums the minimum counts of tuple elements from index @p I onward.
        ///
        /// @return The summed minimum count.
        template <typename T, std::size_t I>
            requires(I < type_count_base_v<T>)
        constexpr auto tuple_type_size_min() -> int
        {
            return subtype_count_min<std::tuple_element_t<I, T>>::value + tuple_type_size_min<T, I + 1>();
        }

        /// @brief Tuples sum the minimum counts of their elements.
        template <typename T>
            requires(tuple_like<T> && !complex_like<T>)
        struct type_count_min<T>
        {
                /// @brief The minimum value count.
                static constexpr int value {tuple_type_size_min<T, 0>()};
        };

        /// @brief Convenience accessor for @ref type_count_min.
        template <typename T> constexpr int type_count_min_v = type_count_min<T>::value;

        /// @brief Minimum value count of a nested type.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct subtype_count_min
        {
                /// @brief The minimum value count.
                static constexpr int value {
                    mutable_container<T>
                        ? ((type_count<T>::value < expected_max_vector_size) ? type_count<T>::value : 0)
                        : type_count_min<T>::value};
        };

        /// @brief How many separate command-line appearances a type expects.
        ///
        /// @tparam T The type to measure.
        template <typename T> struct expected_count
        {
                /// @brief The expected appearance count.
                static constexpr int value {0};
        };

        /// @brief Plain types expect a single appearance.
        template <typename T>
            requires(!mutable_container<T> && !wrapper_like<T> && !std::is_void_v<T>)
        struct expected_count<T>
        {
                /// @brief The expected appearance count.
                static constexpr int value {1};
        };

        /// @brief Containers accept an unbounded number of appearances.
        template <mutable_container T> struct expected_count<T>
        {
                /// @brief The expected appearance count.
                static constexpr int value {expected_max_vector_size};
        };

        /// @brief Wrappers report the expectation of the type they hold.
        template <typename T>
            requires(!mutable_container<T> && wrapper_like<T>)
        struct expected_count<T>
        {
                /// @brief The expected appearance count.
                static constexpr int value {expected_count<typename T::value_type>::value};
        };

        /// @brief Convenience accessor for @ref expected_count.
        template <typename T> constexpr int expected_count_v = expected_count<T>::value;

        /// @brief The classification buckets a type can fall into.
        ///
        /// The numeric values are not arbitrary: several checks compare ranges, so
        /// the string-like categories sit contiguously between
        /// @ref object_category_t::string_assignable and @ref object_category_t::other.
        enum class object_category_t : std::uint8_t
        {
            char_value = 1,             ///< A single character.
            integral_value = 2,         ///< A signed integer.
            unsigned_integral = 4,      ///< An unsigned integer.
            enumeration = 6,            ///< An enumeration.
            boolean_value = 8,          ///< A boolean.
            floating_point = 10,        ///< A floating-point number.
            number_constructible = 12,  ///< Constructible from either `int` or `double`.
            double_constructible = 14,  ///< Constructible from `double` only.
            integer_constructible = 16, ///< Constructible from `int` only.
            string_assignable = 23,     ///< Assignable from `std::string`.
            string_constructible = 24,  ///< Constructible from `std::string`.
            wstring_assignable = 25,    ///< Assignable from `std::wstring`.
            wstring_constructible = 26, ///< Constructible from `std::wstring`.
            other = 45,                 ///< Anything else with a stream operator.
            wrapper_value = 50,         ///< Holds a nested `value_type`.
            complex_number = 60,        ///< A complex number.
            tuple_value = 70,           ///< A tuple or pair.
            container_value = 80,       ///< A sequence or associative container.
        };

#if defined _MSC_VER

        /// @brief Excludes wide-string conversions when classifying narrow strings.
        ///
        /// A type convertible from both `std::string` and `std::wstring` would match
        /// two categories. The tie is broken per platform: MSVC prefers the narrow
        /// form, so the narrow categories exclude anything wide-convertible.
        template <typename T>
        concept wide_string_guard = !std::is_assignable_v<T &, std::wstring> &&
                                    !std::is_constructible_v<T, std::wstring>;

        /// @brief Excludes narrow-string conversions when classifying wide strings.
        template <typename T>
        concept narrow_string_guard = true;

#else

        /// @brief Excludes wide-string conversions when classifying narrow strings.
        template <typename T>
        concept wide_string_guard = true;

        /// @brief Excludes narrow-string conversions when classifying wide strings.
        ///
        /// A type convertible from both `std::string` and `std::wstring` would match
        /// two categories. Outside MSVC the wide form wins, so the wide categories
        /// exclude anything narrow-convertible.
        template <typename T>
        concept narrow_string_guard = !std::is_assignable_v<T &, std::string> &&
                                      !std::is_constructible_v<T, std::string>;

#endif

        /// @brief Matches types none of the ordinary categories claim.
        ///
        /// Not arithmetic, not string-like, not complex, not a container, not an
        /// enumeration. What is left is classified by how it can be constructed.
        template <typename T>
        concept uncommon = !std::is_floating_point_v<T> && !std::is_integral_v<T> &&
                           !std::is_assignable_v<T &, std::string> && !std::is_constructible_v<T, std::string> &&
                           !std::is_assignable_v<T &, std::wstring> && !std::is_constructible_v<T, std::wstring> &&
                           !complex_like<T> && !mutable_container<T> && !std::is_enum_v<T>;

        /// @brief Sorts a type into an @ref object_category_t.
        ///
        /// @tparam T The type to classify.
        template <typename T> struct classify_object
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::other};
        };

        template <typename T>
            requires(std::is_integral_v<T> && !std::same_as<T, char> && std::is_signed_v<T> && !bool_like<T> &&
                     !std::is_enum_v<T>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::integral_value};
        };

        template <typename T>
            requires(std::is_integral_v<T> && std::is_unsigned_v<T> && !std::same_as<T, char> && !bool_like<T>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::unsigned_integral};
        };

        template <typename T>
            requires(std::same_as<T, char> && !std::is_enum_v<T>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::char_value};
        };

        template <bool_like T> struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::boolean_value};
        };

        template <typename T>
            requires std::is_floating_point_v<T>
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::floating_point};
        };

        template <typename T>
            requires(!std::is_floating_point_v<T> && !std::is_integral_v<T> && wide_string_guard<T> &&
                     std::is_assignable_v<T &, std::string>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::string_assignable};
        };

        template <typename T>
            requires(!std::is_floating_point_v<T> && !std::is_integral_v<T> &&
                     !std::is_assignable_v<T &, std::string> && (type_count_v<T> == 1) && wide_string_guard<T> &&
                     std::is_constructible_v<T, std::string>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::string_constructible};
        };

        template <typename T>
            requires(!std::is_floating_point_v<T> && !std::is_integral_v<T> && narrow_string_guard<T> &&
                     std::is_assignable_v<T &, std::wstring>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::wstring_assignable};
        };

        template <typename T>
            requires(!std::is_floating_point_v<T> && !std::is_integral_v<T> &&
                     !std::is_assignable_v<T &, std::wstring> && (type_count_v<T> == 1) && narrow_string_guard<T> &&
                     std::is_constructible_v<T, std::wstring>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::wstring_constructible};
        };

        template <typename T>
            requires std::is_enum_v<T>
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::enumeration};
        };

        template <complex_like T> struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::complex_number};
        };

        template <typename T>
            requires(!mutable_container<T> && wrapper_like<T> && !tuple_like<T> && uncommon<T>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::wrapper_value};
        };

        template <typename T>
            requires(uncommon<T> && type_count_v<T> == 1 && !wrapper_like<T> && direct_constructible<T, double> &&
                     direct_constructible<T, int>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::number_constructible};
        };

        template <typename T>
            requires(uncommon<T> && type_count_v<T> == 1 && !wrapper_like<T> && !direct_constructible<T, double> &&
                     direct_constructible<T, int>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::integer_constructible};
        };

        template <typename T>
            requires(uncommon<T> && type_count_v<T> == 1 && !wrapper_like<T> && direct_constructible<T, double> &&
                     !direct_constructible<T, int>)
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::double_constructible};
        };

        template <typename T>
            requires(tuple_like<T> && ((type_count_v<T> >= 2 && !wrapper_like<T>) ||
                                       (uncommon<T> && !direct_constructible<T, double> &&
                                        !direct_constructible<T, int>) ||
                                       (uncommon<T> && type_count_v<T> >= 2)))
        struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::tuple_value};
        };

        template <mutable_container T> struct classify_object<T>
        {
                /// @brief The category @p T falls into.
                static constexpr object_category_t value {object_category_t::container_value};
        };

        /// @brief Convenience accessor for @ref classify_object.
        template <typename T> constexpr object_category_t classify_object_v = classify_object<T>::value;

        /// @name Category concepts
        ///
        /// One concept per @ref object_category_t, so that the conversion overloads
        /// below can constrain on a name rather than on a comparison.
        ///@{

        /// @brief Matches types classified as a single character.
        template <typename T>
        concept char_value_like = (classify_object_v<T> == object_category_t::char_value);

        /// @brief Matches types classified as a signed integer.
        template <typename T>
        concept integral_value_like = (classify_object_v<T> == object_category_t::integral_value);

        /// @brief Matches types classified as an unsigned integer.
        template <typename T>
        concept unsigned_integral_like = (classify_object_v<T> == object_category_t::unsigned_integral);

        /// @brief Matches types classified as an enumeration.
        template <typename T>
        concept enumeration_like = (classify_object_v<T> == object_category_t::enumeration);

        /// @brief Matches types classified as a boolean.
        template <typename T>
        concept boolean_value_like = (classify_object_v<T> == object_category_t::boolean_value);

        /// @brief Matches types classified as floating point.
        template <typename T>
        concept floating_point_like = (classify_object_v<T> == object_category_t::floating_point);

        /// @brief Matches types constructible from either `int` or `double`.
        template <typename T>
        concept number_constructible_like = (classify_object_v<T> == object_category_t::number_constructible);

        /// @brief Matches types constructible from `double` only.
        template <typename T>
        concept double_constructible_like = (classify_object_v<T> == object_category_t::double_constructible);

        /// @brief Matches types constructible from `int` only.
        template <typename T>
        concept integer_constructible_like = (classify_object_v<T> == object_category_t::integer_constructible);

        /// @brief Matches types assignable from `std::string`.
        template <typename T>
        concept string_assignable_like = (classify_object_v<T> == object_category_t::string_assignable);

        /// @brief Matches types constructible from `std::string`.
        template <typename T>
        concept string_constructible_like = (classify_object_v<T> == object_category_t::string_constructible);

        /// @brief Matches types assignable from `std::wstring`.
        template <typename T>
        concept wstring_assignable_like = (classify_object_v<T> == object_category_t::wstring_assignable);

        /// @brief Matches types constructible from `std::wstring`.
        template <typename T>
        concept wstring_constructible_like = (classify_object_v<T> == object_category_t::wstring_constructible);

        /// @brief Matches types that fell through every other classification.
        template <typename T>
        concept other_like = (classify_object_v<T> == object_category_t::other);

        /// @brief Matches types classified as a wrapper around a nested type.
        template <typename T>
        concept wrapper_value_like = (classify_object_v<T> == object_category_t::wrapper_value);

        /// @brief Matches types classified as a complex number.
        template <typename T>
        concept complex_number_like = (classify_object_v<T> == object_category_t::complex_number);

        /// @brief Matches types classified as a tuple.
        template <typename T>
        concept tuple_value_like = (classify_object_v<T> == object_category_t::tuple_value);

        /// @brief Matches types classified as a container.
        template <typename T>
        concept container_value_like = (classify_object_v<T> == object_category_t::container_value);

        /// @brief Matches every string-like category, plus @ref object_category_t::other.
        ///
        /// These share a single displayed type name, so they share one concept. The
        /// grouping relies on the enumerator values being contiguous.
        template <typename T>
        concept text_like = (classify_object_v<T> >= object_category_t::string_assignable &&
                             classify_object_v<T> <= object_category_t::other);

        ///@}

        /// @brief Returns the name shown for a character option.
        ///
        /// @return `"CHAR"`.
        template <char_value_like T> constexpr auto type_name() -> const char *
        {
            return "CHAR";
        }

        /// @brief Returns the name shown for a signed integer option.
        ///
        /// @return `"INT"`.
        template <typename T>
            requires(integral_value_like<T> || integer_constructible_like<T>)
        constexpr auto type_name() -> const char *
        {
            return "INT";
        }

        /// @brief Returns the name shown for an unsigned integer option.
        ///
        /// @return `"UINT"`.
        template <unsigned_integral_like T> constexpr auto type_name() -> const char *
        {
            return "UINT";
        }

        /// @brief Returns the name shown for a floating-point option.
        ///
        /// @return `"FLOAT"`.
        template <typename T>
            requires(floating_point_like<T> || number_constructible_like<T> || double_constructible_like<T>)
        constexpr auto type_name() -> const char *
        {
            return "FLOAT";
        }

        /// @brief Returns the name shown for an enumeration option.
        ///
        /// @return `"ENUM"`.
        template <enumeration_like T> constexpr auto type_name() -> const char *
        {
            return "ENUM";
        }

        /// @brief Returns the name shown for a boolean option.
        ///
        /// @return `"BOOLEAN"`.
        template <boolean_value_like T> constexpr auto type_name() -> const char *
        {
            return "BOOLEAN";
        }

        /// @brief Returns the name shown for a complex-number option.
        ///
        /// @return `"COMPLEX"`.
        template <complex_number_like T> constexpr auto type_name() -> const char *
        {
            return "COMPLEX";
        }

        /// @brief Returns the name shown for a textual option.
        ///
        /// @return `"TEXT"`.
        template <text_like T> constexpr auto type_name() -> const char *
        {
            return "TEXT";
        }

        /// @brief Returns the bracketed name shown for a multi-element tuple option.
        ///
        /// @return The rendered name.
        template <typename T>
            requires(tuple_value_like<T> && type_count_base_v<T> >= 2)
        auto type_name() -> std::string;

        /// @brief Returns the name of the element type of a container or wrapper.
        ///
        /// @return The rendered name.
        template <typename T>
            requires(container_value_like<T> || wrapper_value_like<T>)
        auto type_name() -> std::string;

        /// @brief Returns the name of a single-element tuple's one element.
        ///
        /// @return The rendered name.
        template <typename T>
            requires(tuple_value_like<T> && type_count_base_v<T> == 1)
        auto type_name() -> std::string
        {
            return type_name<std::decay_t<std::tuple_element_t<0, T>>>();
        }

        /// @brief Terminates the tuple name recursion.
        ///
        /// @return An empty string.
        template <typename T, std::size_t I>
            requires(I == type_count_base_v<T>)
        auto tuple_name() -> std::string
        {
            return std::string {};
        }

        /// @brief Renders the names of tuple elements from index @p I onward.
        ///
        /// @return The rendered names, comma-separated.
        template <typename T, std::size_t I>
            requires(I < type_count_base_v<T>)
        auto tuple_name() -> std::string
        {
            auto str = std::string {type_name<std::decay_t<std::tuple_element_t<I, T>>>()} + ',' +
                       tuple_name<T, I + 1>();
            if (str.back() == ',')
            {
                str.pop_back();
            }
            return str;
        }

        template <typename T>
            requires(tuple_value_like<T> && type_count_base_v<T> >= 2)
        auto type_name() -> std::string
        {
            auto tname = std::string(1, '[') + tuple_name<T, 0>();
            tname.push_back(']');
            return tname;
        }

        template <typename T>
            requires(container_value_like<T> || wrapper_value_like<T>)
        auto type_name() -> std::string
        {
            return type_name<typename T::value_type>();
        }

        /// @brief Parses an unsigned integer, accepting several notations.
        ///
        /// Understands decimal, `0x` hexadecimal, `0o` octal, and `0b` binary, plus
        /// digit-group separators and surrounding whitespace.
        ///
        /// @param[in] input The text to parse.
        /// @param[out] output The value to fill.
        /// @return `true` if the whole string parsed and the value fits in @p T.
        template <typename T>
            requires std::is_unsigned_v<T>
        auto integral_conversion(const std::string &input, T &output) noexcept -> bool
        {
            if (input.empty() || input.front() == '-')
            {
                return false;
            }
            char *val {nullptr};
            errno = 0;
            std::uint64_t output_ll = std::strtoull(input.c_str(), &val, 0);
            if (errno == ERANGE)
            {
                return false;
            }
            output = static_cast<T>(output_ll);
            if (val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll)
            {
                return true;
            }
            val = nullptr;
            const std::int64_t output_sll = std::strtoll(input.c_str(), &val, 0);
            if (val == (input.c_str() + input.size()))
            {
                output = (output_sll < 0) ? static_cast<T>(0) : static_cast<T>(output_sll);
                return (static_cast<std::int64_t>(output) == output_sll);
            }
            const auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                std::string nstring = input;
                for (const auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        std::erase(nstring, separator);
                    }
                }
                return integral_conversion(nstring, output);
            }

            if (std::isspace(static_cast<unsigned char>(input.back())))
            {
                return integral_conversion(trim_copy(input), output);
            }
            if (input.compare(0, 2, "0o") == 0 || input.compare(0, 2, "0O") == 0)
            {
                val = nullptr;
                errno = 0;
                output_ll = std::strtoull(input.c_str() + 2, &val, 8);
                if (errno == ERANGE)
                {
                    return false;
                }
                output = static_cast<T>(output_ll);
                return (val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll);
            }
            if (input.compare(0, 2, "0b") == 0 || input.compare(0, 2, "0B") == 0)
            {
                val = nullptr;
                errno = 0;
                output_ll = std::strtoull(input.c_str() + 2, &val, 2);
                if (errno == ERANGE)
                {
                    return false;
                }
                output = static_cast<T>(output_ll);
                return (val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll);
            }
            return false;
        }

        /// @brief Parses a signed integer, accepting several notations.
        ///
        /// Understands decimal, `0x` hexadecimal, `0o` octal, `0b` binary, the word
        /// `true`, digit-group separators, and surrounding whitespace.
        ///
        /// @param[in] input The text to parse.
        /// @param[out] output The value to fill.
        /// @return `true` if the whole string parsed and the value fits in @p T.
        template <typename T>
            requires std::is_signed_v<T>
        auto integral_conversion(const std::string &input, T &output) noexcept -> bool
        {
            if (input.empty())
            {
                return false;
            }
            char *val = nullptr;
            errno = 0;
            std::int64_t output_ll = std::strtoll(input.c_str(), &val, 0);
            if (errno == ERANGE)
            {
                return false;
            }
            output = static_cast<T>(output_ll);
            if (val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll)
            {
                return true;
            }
            if (input == "true")
            {
                output = static_cast<T>(1);
                return true;
            }
            const auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                for (const auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        std::string nstring = input;
                        std::erase(nstring, separator);
                        return integral_conversion(nstring, output);
                    }
                }
            }
            if (std::isspace(static_cast<unsigned char>(input.back())))
            {
                return integral_conversion(trim_copy(input), output);
            }
            if (input.compare(0, 2, "0o") == 0 || input.compare(0, 2, "0O") == 0)
            {
                val = nullptr;
                errno = 0;
                output_ll = std::strtoll(input.c_str() + 2, &val, 8);
                if (errno == ERANGE)
                {
                    return false;
                }
                output = static_cast<T>(output_ll);
                return (val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll);
            }
            if (input.compare(0, 2, "0b") == 0 || input.compare(0, 2, "0B") == 0)
            {
                val = nullptr;
                errno = 0;
                output_ll = std::strtoll(input.c_str() + 2, &val, 2);
                if (errno == ERANGE)
                {
                    return false;
                }
                output = static_cast<T>(output_ll);
                return (val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll);
            }
            return false;
        }

        /// @brief Interprets a string as a tri-state flag value.
        ///
        /// Accepts `true`/`false`, `on`/`off`, `yes`/`no`, `enable`/`disable`, single
        /// characters such as `t`, `y`, `+`, `f`, `n`, `-`, single digits, and plain
        /// integers.
        ///
        /// @param val The text to interpret.
        /// @return A positive value for true, negative for false. On failure, sets
        /// `errno` to `EINVAL`; callers must clear `errno` before calling.
        auto to_flag_value(std::string val) noexcept -> std::int64_t
        {
            static constexpr std::string_view true_string {"true"};
            static constexpr std::string_view false_string {"false"};

            if (val == true_string)
            {
                return 1;
            }
            if (val == false_string)
            {
                return -1;
            }
            val = to_lower(val);
            std::int64_t ret = 0;
            if (val.size() == 1)
            {
                if (val[0] >= '1' && val[0] <= '9')
                {
                    return (static_cast<std::int64_t>(val[0]) - '0');
                }
                switch (val[0])
                {
                case '0':
                case 'f':
                case 'n':
                case '-':
                    ret = -1;
                    break;
                case 't':
                case 'y':
                case '+':
                    ret = 1;
                    break;
                default:
                    errno = EINVAL;
                    return -1;
                }
                return ret;
            }
            if (val == true_string || val == "on" || val == "yes" || val == "enable")
            {
                ret = 1;
            }
            else if (val == false_string || val == "off" || val == "no" || val == "disable")
            {
                ret = -1;
            }
            else
            {
                char *loc_ptr {nullptr};
                ret = std::strtoll(val.c_str(), &loc_ptr, 0);
                if (loc_ptr != (val.c_str() + val.size()) && errno == 0)
                {
                    errno = EINVAL;
                }
            }
            return ret;
        }

        /// @name Single-value conversion
        ///
        /// @ref lexical_cast turns one command-line token into one value. There is
        /// an overload per @ref object_category_t; exactly one is viable for any
        /// given type.
        ///@{

        /// @brief Converts a token to an integer.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename T>
            requires(integral_value_like<T> || unsigned_integral_like<T>)
        auto lexical_cast(const std::string &input, T &output) -> bool
        {
            return integral_conversion(input, output);
        }

        /// @brief Converts a token to a character, accepting a numeric code point.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <char_value_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            if (input.size() == 1)
            {
                output = static_cast<T>(input[0]);
                return true;
            }
            std::int8_t res {0};
            const bool result = integral_conversion(input, res);
            if (result)
            {
                output = static_cast<T>(res);
            }
            return result;
        }

        /// @brief Converts a token to a boolean.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <boolean_value_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            errno = 0;
            const auto out = to_flag_value(input);
            if (errno == 0)
            {
                output = (out > 0);
            }
            else if (errno == ERANGE)
            {
                output = (input[0] != '-');
            }
            else
            {
                return false;
            }
            return true;
        }

        /// @brief Converts a token to a floating-point value.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <floating_point_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            if (input.empty())
            {
                return false;
            }
            char *val = nullptr;
            const auto output_ld = std::strtold(input.c_str(), &val);
            output = static_cast<T>(output_ld);
            if (val == (input.c_str() + input.size()))
            {
                return true;
            }
            while (std::isspace(static_cast<unsigned char>(*val)))
            {
                ++val;
                if (val == (input.c_str() + input.size()))
                {
                    return true;
                }
            }

            const auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                for (const auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        std::string nstring = input;
                        std::erase(nstring, separator);
                        return lexical_cast(nstring, output);
                    }
                }
            }
            return false;
        }

        /// @brief Converts a token such as `"3+4i"` to a complex number.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <complex_number_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            using xc_t = typename wrapped_type<T, double>::type;
            xc_t x {0.0};
            xc_t y {0.0};
            auto str1 = input;
            bool worked = false;
            const auto nloc = str1.find_last_of("+-");
            if (nloc != std::string::npos && nloc > 0)
            {
                worked = lexical_cast(str1.substr(0, nloc), x);
                str1 = str1.substr(nloc);
                if (str1.back() == 'i' || str1.back() == 'j')
                {
                    str1.pop_back();
                }
                worked = worked && lexical_cast(str1, y);
            }
            else
            {
                if (str1.back() == 'i' || str1.back() == 'j')
                {
                    str1.pop_back();
                    worked = lexical_cast(str1, y);
                    x = xc_t {0};
                }
                else
                {
                    worked = lexical_cast(str1, x);
                    y = xc_t {0};
                }
            }
            if (worked)
            {
                output = T {x, y};
                return worked;
            }
            return from_stream(input, output);
        }

        /// @brief Assigns a token directly to a string-assignable type.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return Always `true`.
        template <string_assignable_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            output = input;
            return true;
        }

        /// @brief Constructs a string-constructible type from a token.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return Always `true`.
        template <string_constructible_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            output = T(input);
            return true;
        }

        /// @brief Widens a token and assigns it.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return Always `true`.
        template <wstring_assignable_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            output = widen(input);
            return true;
        }

        /// @brief Widens a token and constructs from it.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return Always `true`.
        template <wstring_constructible_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            output = T {widen(input)};
            return true;
        }

        /// @brief Converts a token to an enumerator via its underlying type.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <enumeration_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            std::underlying_type_t<T> val {};
            if (!integral_conversion(input, val))
            {
                return false;
            }
            output = static_cast<T>(val);
            return true;
        }

        /// @brief Converts a token into a wrapper by assigning the inner value.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename T>
            requires(wrapper_value_like<T> && std::is_assignable_v<T &, typename T::value_type>)
        auto lexical_cast(const std::string &input, T &output) -> bool
        {
            typename T::value_type val;
            if (lexical_cast(input, val))
            {
                output = val;
                return true;
            }
            return from_stream(input, output);
        }

        /// @brief Converts a token into a wrapper by rebuilding the wrapper.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename T>
            requires(wrapper_value_like<T> && !std::is_assignable_v<T &, typename T::value_type> &&
                     std::is_assignable_v<T &, T>)
        auto lexical_cast(const std::string &input, T &output) -> bool
        {
            typename T::value_type val;
            if (lexical_cast(input, val))
            {
                output = T {val};
                return true;
            }
            return from_stream(input, output);
        }

        /// @brief Converts a token to a type constructible from `int` or `double`.
        ///
        /// The integer form is tried first, so an exact integer does not go through
        /// a floating-point round trip.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <number_constructible_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            int val = 0;
            if (integral_conversion(input, val))
            {
                output = T(val);
                return true;
            }

            double dval = 0.0;
            if (lexical_cast(input, dval))
            {
                output = T {dval};
                return true;
            }

            return from_stream(input, output);
        }

        /// @brief Converts a token to a type constructible from `int`.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <integer_constructible_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            int val = 0;
            if (integral_conversion(input, val))
            {
                output = T(val);
                return true;
            }
            return from_stream(input, output);
        }

        /// @brief Converts a token to a type constructible from `double`.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <double_constructible_like T> auto lexical_cast(const std::string &input, T &output) -> bool
        {
            double val = 0.0;
            if (lexical_cast(input, val))
            {
                output = T {val};
                return true;
            }
            return from_stream(input, output);
        }

        /// @brief Converts a token to an unclassified type assignable from `int`.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename T>
            requires(other_like<T> && std::is_assignable_v<T &, int>)
        auto lexical_cast(const std::string &input, T &output) -> bool
        {
            int val = 0;
            if (integral_conversion(input, val))
            {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4800)
#endif
                output = val;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
                return true;
            }
            return from_stream(input, output);
        }

        /// @brief Converts a token to an unclassified type using its `>>` operator.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename T>
            requires(other_like<T> && !std::is_assignable_v<T &, int> && istreamable<T>)
        auto lexical_cast(const std::string &input, T &output) -> bool
        {
            return from_stream(input, output);
        }

        /// @brief Rejects types with no available conversion.
        ///
        /// Instantiating this overload is always an error. It exists so that the
        /// failure is reported as one clear message rather than as an overload
        /// resolution failure.
        ///
        /// @return Never returns; the assertion always fires.
        template <typename T>
            requires(other_like<T> && !std::is_assignable_v<T &, int> && !istreamable<T> &&
                     !adl_detail::lexical_castable<T>)
        auto lexical_cast(const std::string & /*input*/, T & /*output*/) -> bool
        {
            static_assert(false,
                          "option object type must have a lexical cast overload or streaming input operator(>>) "
                          "defined, if it is convertible from another type use the add_option<T, XC>(...) with XC "
                          "being the known type");
            return false;
        }

        ///@}

        /// @brief Matches the four string-like categories as a group.
        template <typename T>
        concept string_like_category = string_assignable_like<T> || string_constructible_like<T> ||
                                       wstring_assignable_like<T> || wstring_constructible_like<T>;

        /// @name Assignment
        ///
        /// @ref lexical_assign converts a token to `convert_to_t` and then gets that
        /// value into an `assign_to_t`, which is not always the same type.
        ///@{

        /// @brief Assigns a token to a string-like destination.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(std::same_as<assign_to_t, convert_to_t> && string_like_category<assign_to_t>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            return lexical_cast(input, output);
        }

        /// @brief Assigns a token to a self-assignable destination.
        ///
        /// An empty token yields a value-initialised result rather than a parse failure.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(std::same_as<assign_to_t, convert_to_t> && std::is_assignable_v<assign_to_t &, assign_to_t> &&
                     !string_like_category<assign_to_t>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            if (input.empty())
            {
                output = assign_to_t {};
                return true;
            }

            return lexical_cast(input, output);
        }

        /// @brief Assigns a token to a wrapper that is not self-assignable.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(std::same_as<assign_to_t, convert_to_t> && !std::is_assignable_v<assign_to_t &, assign_to_t> &&
                     wrapper_value_like<assign_to_t>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            if (input.empty())
            {
                typename assign_to_t::value_type empty_val {};
                output = empty_val;
                return true;
            }
            return lexical_cast(input, output);
        }

        /// @brief Assigns a token to a destination reachable only through `int`.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(std::same_as<assign_to_t, convert_to_t> && !std::is_assignable_v<assign_to_t &, assign_to_t> &&
                     !wrapper_value_like<assign_to_t> && std::is_assignable_v<assign_to_t &, int>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            if (input.empty())
            {
                output = 0;
                return true;
            }
            int val {0};
            if (lexical_cast(input, val))
            {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
                output = val;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
                return true;
            }
            return false;
        }

        /// @brief Converts through an intermediate type, then assigns.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(!std::same_as<assign_to_t, convert_to_t> && std::is_assignable_v<assign_to_t &, convert_to_t &>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            convert_to_t val {};
            const bool parse_result = (!input.empty()) ? lexical_cast(input, val) : true;
            if (parse_result)
            {
                output = val;
            }
            return parse_result;
        }

        /// @brief Converts through an intermediate type, then constructs.
        ///
        /// @param[in] input The token to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(!std::same_as<assign_to_t, convert_to_t> && !std::is_assignable_v<assign_to_t &, convert_to_t &> &&
                     std::is_move_assignable_v<assign_to_t>)
        auto lexical_assign(const std::string &input, assign_to_t &output) -> bool
        {
            convert_to_t val {};
            const bool parse_result = input.empty() ? true : lexical_cast(input, val);
            if (parse_result)
            {
                output = assign_to_t(val);
            }
            return parse_result;
        }

        ///@}
        /// @name List conversion
        ///
        /// @ref lexical_conversion turns a whole list of tokens into one value,
        /// distributing them across tuple elements or container entries as needed.
        ///@{

        /// @brief Converts a single-value type from the first token.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(classify_object_v<convert_to_t> <= object_category_t::other &&
                     classify_object_v<assign_to_t> <= object_category_t::wrapper_value)
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            return lexical_assign<assign_to_t, convert_to_t>(strings[0], output);
        }

        /// @brief Converts a two-element tuple from up to two tokens.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires((type_count_v<assign_to_t> <= 2) && expected_count_v<assign_to_t> == 1 && tuple_like<convert_to_t> &&
                     type_count_base_v<convert_to_t> == 2)
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            using first_t = std::remove_const_t<std::tuple_element_t<0, convert_to_t>>;
            using second_t = std::tuple_element_t<1, convert_to_t>;

            first_t v1;
            second_t v2 {};
            bool retval = lexical_assign<first_t, first_t>(strings[0], v1);
            retval = retval && lexical_assign<second_t, second_t>((strings.size() > 1) ? strings[1] : std::string {},
                                                                  v2);
            if (retval)
            {
                output = assign_to_t {v1, v2};
            }
            return retval;
        }

        /// @brief Converts a container of single-value elements.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The container to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && mutable_container<convert_to_t> && type_count_v<convert_to_t> == 1)
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            output.erase(output.begin(), output.end());
            if (strings.empty())
            {
                return true;
            }
            if (strings.size() == 1 && strings[0] == "{}")
            {
                return true;
            }
            bool skip_remaining = false;
            if (strings.size() == 2 && strings[0] == "{}" && is_separator(strings[1]))
            {
                skip_remaining = true;
            }
            for (const auto &elem : strings)
            {
                typename assign_to_t::value_type out;
                const bool retval =
                    lexical_assign<typename assign_to_t::value_type, typename convert_to_t::value_type>(elem, out);
                if (!retval)
                {
                    return false;
                }
                output.insert(output.end(), std::move(out));
                if (skip_remaining)
                {
                    break;
                }
            }
            return (!output.empty());
        }

        /// @brief Converts a complex number from one or two tokens.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires complex_like<convert_to_t>
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            if (strings.size() >= 2 && !strings[1].empty())
            {
                using xc2_t = typename wrapped_type<convert_to_t, double>::type;
                xc2_t x {0.0};
                xc2_t y {0.0};
                auto str1 = strings[1];
                if (str1.back() == 'i' || str1.back() == 'j')
                {
                    str1.pop_back();
                }
                const auto worked = lexical_cast(strings[0], x) && lexical_cast(str1, y);
                if (worked)
                {
                    output = convert_to_t {x, y};
                }
                return worked;
            }
            return lexical_assign<assign_to_t, convert_to_t>(strings[0], output);
        }

        /// @brief Fills a container, one element per token.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The container to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && (expected_count_v<convert_to_t> == 1) &&
                     (type_count_v<convert_to_t> == 1))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            bool retval = true;
            output.clear();
            output.reserve(strings.size());
            for (const auto &elem : strings)
            {
                output.emplace_back();
                retval = retval && lexical_assign<typename assign_to_t::value_type, convert_to_t>(elem, output.back());
            }
            return (!output.empty()) && retval;
        }

        /// @brief Converts a container of pairs, consuming tokens two at a time.
        ///
        /// @param[in] strings The tokens to convert; consumed as it goes.
        /// @param[out] output The container to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && mutable_container<convert_to_t> &&
                     type_count_base_v<convert_to_t> == 2)
        auto lexical_conversion(std::vector<std::string> strings, assign_to_t &output) -> bool;

        /// @brief Converts a container whose elements consume several tokens each.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The container to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && mutable_container<convert_to_t> &&
                     type_count_base_v<convert_to_t> != 2 &&
                     ((type_count_v<convert_to_t> > 2) || (type_count_v<convert_to_t> > type_count_base_v<convert_to_t>)))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool;

        /// @brief Converts a tuple whose elements are themselves compound.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(tuple_like<assign_to_t> && tuple_like<convert_to_t> &&
                     (type_count_base_v<convert_to_t> != type_count_v<convert_to_t> || type_count_v<convert_to_t> > 2))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool;

        /// @brief Converts a compound source into a scalar destination.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(!tuple_like<assign_to_t> && !mutable_container<assign_to_t> && !wrapper_value_like<convert_to_t> &&
                     (mutable_container<convert_to_t> || type_count_v<convert_to_t> > 2))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            if (strings.size() > 1 || (!strings.empty() && !(strings.front().empty())))
            {
                convert_to_t val;
                const auto retval = lexical_conversion<convert_to_t, convert_to_t>(strings, val);
                output = assign_to_t {val};
                return retval;
            }
            output = assign_to_t {};
            return true;
        }

        /// @brief Terminates the tuple conversion recursion.
        ///
        /// @return Always `true`.
        template <typename assign_to_t, typename convert_to_t, std::size_t I>
            requires(I >= type_count_base_v<assign_to_t>)
        auto tuple_conversion(const std::vector<std::string> & /*strings*/, assign_to_t & /*output*/) -> bool
        {
            return true;
        }

        /// @brief Consumes one token for a single-value tuple element.
        ///
        /// @param[in,out] strings The remaining tokens; the consumed one is removed.
        /// @param[out] output The element to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(!mutable_container<convert_to_t> && type_count_v<convert_to_t> == 1)
        auto tuple_type_conversion(std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            const auto retval = lexical_assign<assign_to_t, convert_to_t>(strings[0], output);
            strings.erase(strings.begin());
            return retval;
        }

        /// @brief Consumes a fixed number of tokens for a compound tuple element.
        ///
        /// @param[in,out] strings The remaining tokens; the consumed ones are removed.
        /// @param[out] output The element to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(!mutable_container<convert_to_t> && (type_count_v<convert_to_t> > 1) &&
                     type_count_v<convert_to_t> == type_count_min_v<convert_to_t>)
        auto tuple_type_conversion(std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            const auto retval = lexical_conversion<assign_to_t, convert_to_t>(strings, output);
            strings.erase(strings.begin(), strings.begin() + type_count_v<convert_to_t>);
            return retval;
        }

        /// @brief Consumes a variable number of tokens, stopping at a separator.
        ///
        /// @param[in,out] strings The remaining tokens; the consumed ones are removed.
        /// @param[out] output The element to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<convert_to_t> || type_count_v<convert_to_t> != type_count_min_v<convert_to_t>)
        auto tuple_type_conversion(std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            std::size_t index {subtype_count_min<convert_to_t>::value};
            const std::size_t mx_count {subtype_count<convert_to_t>::value};
            const std::size_t mx {(std::min)(mx_count, strings.size() - 1)};

            while (index < mx)
            {
                if (is_separator(strings[index]))
                {
                    break;
                }
                ++index;
            }
            const bool retval = lexical_conversion<assign_to_t, convert_to_t>(
                std::vector<std::string>(strings.begin(), strings.begin() + static_cast<std::ptrdiff_t>(index)),
                output);
            if (strings.size() > index)
            {
                strings.erase(strings.begin(), strings.begin() + static_cast<std::ptrdiff_t>(index) + 1);
            }
            else
            {
                strings.clear();
            }
            return retval;
        }

        /// @brief Fills tuple element @p I, then recurses to the next.
        ///
        /// @param[in] strings The remaining tokens.
        /// @param[out] output The tuple to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t, std::size_t I>
            requires(I < type_count_base_v<assign_to_t>)
        auto tuple_conversion(std::vector<std::string> strings, assign_to_t &output) -> bool
        {
            bool retval = true;
            using convert_element_t =
                std::conditional_t<tuple_like<convert_to_t>, std::tuple_element_t<I, convert_to_t>, convert_to_t>;
            if (!strings.empty())
            {
                retval = retval && tuple_type_conversion<std::tuple_element_t<I, assign_to_t>, convert_element_t>(
                                       strings, std::get<I>(output));
            }
            retval = retval && tuple_conversion<assign_to_t, convert_to_t, I + 1>(std::move(strings), output);
            return retval;
        }

        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && mutable_container<convert_to_t> &&
                     type_count_base_v<convert_to_t> == 2)
        auto lexical_conversion(std::vector<std::string> strings, assign_to_t &output) -> bool
        {
            output.clear();
            while (!strings.empty())
            {
                std::remove_const_t<std::tuple_element_t<0, typename convert_to_t::value_type>> v1;
                std::tuple_element_t<1, typename convert_to_t::value_type> v2;

                bool retval = tuple_type_conversion<decltype(v1), decltype(v1)>(strings, v1);
                if (!strings.empty())
                {
                    retval = retval && tuple_type_conversion<decltype(v2), decltype(v2)>(strings, v2);
                }
                if (retval)
                {
                    output.insert(output.end(), typename assign_to_t::value_type {v1, v2});
                }
                else
                {
                    return false;
                }
            }
            return (!output.empty());
        }

        template <typename assign_to_t, typename convert_to_t>
            requires(tuple_like<assign_to_t> && tuple_like<convert_to_t> &&
                     (type_count_base_v<convert_to_t> != type_count_v<convert_to_t> || type_count_v<convert_to_t> > 2))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            static_assert(!tuple_like<convert_to_t> || type_count_base_v<assign_to_t> == type_count_base_v<convert_to_t>,
                          "if the conversion type is defined as a tuple it must be the same size as the type you are "
                          "converting to");
            return tuple_conversion<assign_to_t, convert_to_t, 0>(strings, output);
        }

        template <typename assign_to_t, typename convert_to_t>
            requires(mutable_container<assign_to_t> && mutable_container<convert_to_t> &&
                     type_count_base_v<convert_to_t> != 2 &&
                     ((type_count_v<convert_to_t> > 2) || (type_count_v<convert_to_t> > type_count_base_v<convert_to_t>)))
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            bool retval = true;
            output.clear();
            std::vector<std::string> temp;
            std::size_t ii {0};
            std::size_t icount {0};
            const std::size_t xcm {static_cast<std::size_t>(type_count_v<convert_to_t>)};
            const auto ii_max = strings.size();

            while (ii < ii_max)
            {
                temp.push_back(strings[ii]);
                ++ii;
                ++icount;
                if (icount == xcm || is_separator(temp.back()) || ii == ii_max)
                {
                    if (static_cast<int>(xcm) > type_count_min_v<convert_to_t> && is_separator(temp.back()))
                    {
                        temp.pop_back();
                    }
                    typename assign_to_t::value_type temp_out;
                    retval = retval &&
                             lexical_conversion<typename assign_to_t::value_type, typename convert_to_t::value_type>(
                                 temp, temp_out);
                    temp.clear();
                    if (!retval)
                    {
                        return false;
                    }
                    output.insert(output.end(), std::move(temp_out));
                    icount = 0;
                }
            }
            return retval;
        }

        /// @brief Converts into a wrapper that can be rebuilt wholesale.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(wrapper_value_like<convert_to_t> && std::is_assignable_v<convert_to_t &, convert_to_t>)
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            if (strings.empty() || strings.front().empty())
            {
                output = convert_to_t {};
                return true;
            }
            typename convert_to_t::value_type val;
            if (lexical_conversion<typename convert_to_t::value_type, typename convert_to_t::value_type>(strings, val))
            {
                output = convert_to_t {val};
                return true;
            }
            return false;
        }

        /// @brief Converts into the value held by a wrapper.
        ///
        /// @param[in] strings The tokens to convert.
        /// @param[out] output The value to fill.
        /// @return `true` on success.
        template <typename assign_to_t, typename convert_to_t>
            requires(wrapper_value_like<convert_to_t> && !std::is_assignable_v<assign_to_t &, convert_to_t>)
        auto lexical_conversion(const std::vector<std::string> &strings, assign_to_t &output) -> bool
        {
            using convert_t = typename convert_to_t::value_type;
            if (strings.empty() || strings.front().empty())
            {
                output = convert_t {};
                return true;
            }
            convert_t val;
            if (lexical_conversion<typename convert_to_t::value_type, typename convert_to_t::value_type>(strings, val))
            {
                output = val;
                return true;
            }
            return false;
        }

        ///@}

        /// @brief Sums a list of values, falling back to concatenation.
        ///
        /// Each entry is read as a number, or failing that as a flag value. If any
        /// entry is neither, the entries are concatenated instead. This backs the
        /// `sum` multi-option policy, which has to work for both numbers and text.
        ///
        /// @param values The values to sum.
        /// @return The sum rendered to sixteen significant digits, or the
        /// concatenation of every input.
        auto sum_string_vector(const std::vector<std::string> &values) -> std::string
        {
            double val {0.0};
            bool fail {false};
            std::string output;

            for (const auto &arg : values)
            {
                double tv {0.0};
                const auto comp = lexical_cast(arg, tv);
                if (!comp)
                {
                    errno = 0;
                    const auto fv = to_flag_value(arg);
                    fail = (errno != 0);
                    if (fail)
                    {
                        break;
                    }
                    tv = static_cast<double>(fv);
                }
                val += tv;
            }

            if (fail)
            {
                for (const auto &arg : values)
                {
                    output.append(arg);
                }
            }
            else
            {
                output = std::format("{:.16g}", val);
            }
            return output;
        }

    } // namespace detail

} // namespace cli
