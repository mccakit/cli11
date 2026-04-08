module;
// C standard library macros must be included in the global module fragment
#include <cerrno>

export module cli11:type_tools;

import std;
import :encoding;
import :string_tools;

export namespace CLI
{

    // Utilities for type enabling
    namespace detail
    {
        enum class enabler : std::uint8_t
        {
        };
        constexpr enabler dummy = {};
    } // namespace detail

    template <bool B, class T = void> using enable_if_t = typename std::enable_if<B, T>::type;

    template <typename... Ts> struct make_void
    {
            using type = void;
    };

    template <typename... Ts> using void_t = typename make_void<Ts...>::type;

    template <bool B, class T, class F> using conditional_t = typename std::conditional<B, T, F>::type;

    template <typename T> struct is_bool : std::false_type
    {
    };
    template <> struct is_bool<bool> : std::true_type
    {
    };

    template <typename T> struct is_shared_ptr : std::false_type
    {
    };
    template <typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
    {
    };
    template <typename T> struct is_shared_ptr<const std::shared_ptr<T>> : std::true_type
    {
    };

    template <typename T> struct is_copyable_ptr
    {
            static bool const value = is_shared_ptr<T>::value || std::is_pointer<T>::value;
    };

    template <typename T> struct IsMemberType
    {
            using type = T;
    };
    template <> struct IsMemberType<const char *>
    {
            using type = std::string;
    };

    namespace adl_detail
    {
        template <typename T, typename S = std::string> class is_lexical_castable
        {
                template <typename TT, typename SS>
                static auto test(int)
                    -> decltype(lexical_cast(std::declval<const SS &>(), std::declval<TT &>()), std::true_type());
                template <typename, typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value = decltype(test<T, S>(0))::value;
        };
    } // namespace adl_detail

    namespace detail
    {

        template <typename T, typename Enable = void> struct element_type
        {
                using type = T;
        };
        template <typename T> struct element_type<T, typename std::enable_if<is_copyable_ptr<T>::value>::type>
        {
                using type = typename std::pointer_traits<T>::element_type;
        };

        template <typename T> struct element_value_type
        {
                using type = typename element_type<T>::type::value_type;
        };

        template <typename T, typename _ = void> struct pair_adaptor : std::false_type
        {
                using value_type = typename T::value_type;
                using first_type = typename std::remove_const<value_type>::type;
                using second_type = typename std::remove_const<value_type>::type;

                template <typename Q> static auto first(Q &&pair_value) -> decltype(std::forward<Q>(pair_value))
                {
                    return std::forward<Q>(pair_value);
                }
                template <typename Q> static auto second(Q &&pair_value) -> decltype(std::forward<Q>(pair_value))
                {
                    return std::forward<Q>(pair_value);
                }
        };

        template <typename T>
        struct pair_adaptor<
            T,
            conditional_t<false, void_t<typename T::value_type::first_type, typename T::value_type::second_type>, void>>
            : std::true_type
        {
                using value_type = typename T::value_type;
                using first_type = typename std::remove_const<typename value_type::first_type>::type;
                using second_type = typename std::remove_const<typename value_type::second_type>::type;

                template <typename Q>
                static auto first(Q &&pair_value) -> decltype(std::get<0>(std::forward<Q>(pair_value)))
                {
                    return std::get<0>(std::forward<Q>(pair_value));
                }
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
        template <typename T, typename C> class is_direct_constructible
        {
                template <typename TT, typename CC>
                static auto test(int, std::true_type) -> decltype(
#ifdef __CUDACC__
#ifdef __NVCC_DIAG_PRAGMA_SUPPORT__
#pragma nv_diag_suppress 2361
#else
#pragma diag_suppress 2361
#endif
#endif
                    TT {std::declval<CC>()}
#ifdef __CUDACC__
#ifdef __NVCC_DIAG_PRAGMA_SUPPORT__
#pragma nv_diag_default 2361
#else
#pragma diag_default 2361
#endif
#endif
                    ,
                    std::is_move_assignable<TT>());

                template <typename TT, typename CC> static auto test(int, std::false_type) -> std::false_type;
                template <typename, typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value =
                    decltype(test<T, C>(0, typename std::is_constructible<T, C>::type()))::value;
        };
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

        template <typename T, typename S = std::ostringstream> class is_ostreamable
        {
                template <typename TT, typename SS>
                static auto test(int) -> decltype(std::declval<SS &>() << std::declval<TT>(), std::true_type());
                template <typename, typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value = decltype(test<T, S>(0))::value;
        };

        template <typename T, typename S = std::istringstream> class is_istreamable
        {
                template <typename TT, typename SS>
                static auto test(int) -> decltype(std::declval<SS &>() >> std::declval<TT &>(), std::true_type());
                template <typename, typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value = decltype(test<T, S>(0))::value;
        };

        template <typename T> class is_complex
        {
                template <typename TT>
                static auto test(int)
                    -> decltype(std::declval<TT>().real(), std::declval<TT>().imag(), std::true_type());
                template <typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value = decltype(test<T>(0))::value;
        };

        template <typename T, enable_if_t<is_istreamable<T>::value, detail::enabler> = detail::dummy>
        bool from_stream(const std::string &istring, T &obj)
        {
            std::istringstream is;
            is.str(istring);
            is >> obj;
            return !is.fail() && !is.rdbuf()->in_avail();
        }

        template <typename T, enable_if_t<!is_istreamable<T>::value, detail::enabler> = detail::dummy>
        bool from_stream(const std::string & /*istring*/, T & /*obj*/)
        {
            return false;
        }

        template <typename T, typename _ = void> struct is_mutable_container : std::false_type
        {
        };

        template <typename T>
        struct is_mutable_container<
            T,
            conditional_t<false,
                          void_t<typename T::value_type,
                                 decltype(std::declval<T>().end()),
                                 decltype(std::declval<T>().clear()),
                                 decltype(std::declval<T>().insert(std::declval<decltype(std::declval<T>().end())>(),
                                                                   std::declval<const typename T::value_type &>()))>,
                          void>> : public conditional_t<std::is_constructible<T, std::string>::value ||
                                                            std::is_constructible<T, std::wstring>::value,
                                                        std::false_type,
                                                        std::true_type>
        {
        };

        template <typename T, typename _ = void> struct is_readable_container : std::false_type
        {
        };

        template <typename T>
        struct is_readable_container<
            T,
            conditional_t<false, void_t<decltype(std::declval<T>().end()), decltype(std::declval<T>().begin())>, void>>
            : public std::true_type
        {
        };

        template <typename T, typename _ = void> struct is_wrapper : std::false_type
        {
        };

        template <typename T>
        struct is_wrapper<T, conditional_t<false, void_t<typename T::value_type>, void>> : public std::true_type
        {
        };

        template <typename S> class is_tuple_like
        {
                template <typename SS, enable_if_t<!is_complex<SS>::value, detail::enabler> = detail::dummy>
                static auto test(int)
                    -> decltype(std::tuple_size<typename std::decay<SS>::type>::value, std::true_type {});
                template <typename> static auto test(...) -> std::false_type;

            public:
                static constexpr bool value = decltype(test<S>(0))::value;
        };

        template <typename T, typename Enable = void> struct type_count_base
        {
                static const int value {0};
        };

        template <typename T>
        struct type_count_base<T,
                               typename std::enable_if<!is_tuple_like<T>::value && !is_mutable_container<T>::value &&
                                                       !std::is_void<T>::value>::type>
        {
                static constexpr int value {1};
        };

        template <typename T>
        struct type_count_base<
            T,
            typename std::enable_if<is_tuple_like<T>::value && !is_mutable_container<T>::value>::type>
        {
                static constexpr int value {std::tuple_size<typename std::decay<T>::type>::value};
        };

        template <typename T> struct type_count_base<T, typename std::enable_if<is_mutable_container<T>::value>::type>
        {
                static constexpr int value {type_count_base<typename T::value_type>::value};
        };

        template <typename T, enable_if_t<std::is_convertible<T, std::string>::value, detail::enabler> = detail::dummy>
        auto to_string(T &&value) -> decltype(std::forward<T>(value))
        {
            return std::forward<T>(value);
        }

        template <
            typename T,
            enable_if_t<std::is_constructible<std::string, T>::value && !std::is_convertible<T, std::string>::value,
                        detail::enabler> = detail::dummy>
        std::string to_string(T &&value)
        {
            return std::string(value);
        }

        template <typename T,
                  enable_if_t<!std::is_convertible<T, std::string>::value &&
                                  !std::is_constructible<std::string, T>::value && is_ostreamable<T>::value,
                              detail::enabler> = detail::dummy>
        std::string to_string(T &&value)
        {
            std::stringstream stream;
            stream << value;
            return stream.str();
        }

        template <
            typename T,
            enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                            !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value == 1,
                        detail::enabler> = detail::dummy>
        std::string to_string(T &&value);

        template <
            typename T,
            enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                            !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value >= 2,
                        detail::enabler> = detail::dummy>
        std::string to_string(T &&value);

        template <typename T,
                  enable_if_t<!std::is_convertible<T, std::string>::value &&
                                  !std::is_constructible<std::string, T>::value && !is_ostreamable<T>::value &&
                                  !is_readable_container<typename std::remove_const<T>::type>::value &&
                                  !is_tuple_like<T>::value,
                              detail::enabler> = detail::dummy>
        std::string to_string(T &&)
        {
            return {};
        }

        template <
            typename T,
            enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                            !is_ostreamable<T>::value && is_readable_container<T>::value && !is_tuple_like<T>::value,
                        detail::enabler> = detail::dummy>
        std::string to_string(T &&variable)
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

        template <typename T, std::size_t I>
        typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_value_string(T && /*value*/);

        template <typename T, std::size_t I>
        typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_value_string(T &&value);

        template <
            typename T,
            enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                            !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value == 1,
                        detail::enabler>>
        std::string to_string(T &&value)
        {
            return to_string(std::get<0>(value));
        }

        template <
            typename T,
            enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                            !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value >= 2,
                        detail::enabler>>
        std::string to_string(T &&value)
        {
            auto tname = std::string(1, '[') + tuple_value_string<T, 0>(value);
            tname.push_back(']');
            return tname;
        }

        template <typename T, std::size_t I>
        typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_value_string(T && /*value*/)
        {
            return std::string {};
        }

        template <typename T, std::size_t I>
        typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_value_string(T &&value)
        {
            auto str = std::string {to_string(std::get<I>(value))} + ',' + tuple_value_string<T, I + 1>(value);
            if (str.back() == ',')
                str.pop_back();
            return str;
        }

        template <typename T1,
                  typename T2,
                  typename T,
                  enable_if_t<std::is_same<T1, T2>::value, detail::enabler> = detail::dummy>
        auto checked_to_string(T &&value) -> decltype(to_string(std::forward<T>(value)))
        {
            return to_string(std::forward<T>(value));
        }

        template <typename T1,
                  typename T2,
                  typename T,
                  enable_if_t<!std::is_same<T1, T2>::value, detail::enabler> = detail::dummy>
        std::string checked_to_string(T &&)
        {
            return std::string {};
        }

        template <typename T, enable_if_t<std::is_arithmetic<T>::value, detail::enabler> = detail::dummy>
        std::string value_string(const T &value)
        {
            return std::to_string(value);
        }

        template <typename T, enable_if_t<std::is_enum<T>::value, detail::enabler> = detail::dummy>
        std::string value_string(const T &value)
        {
            return std::to_string(static_cast<typename std::underlying_type<T>::type>(value));
        }

        template <
            typename T,
            enable_if_t<!std::is_enum<T>::value && !std::is_arithmetic<T>::value, detail::enabler> = detail::dummy>
        auto value_string(const T &value) -> decltype(to_string(value))
        {
            return to_string(value);
        }

        template <typename T, typename def, typename Enable = void> struct wrapped_type
        {
                using type = def;
        };

        template <typename T, typename def>
        struct wrapped_type<T, def, typename std::enable_if<is_wrapper<T>::value>::type>
        {
                using type = typename T::value_type;
        };

        template <typename T> struct subtype_count;
        template <typename T> struct subtype_count_min;

        template <typename T, typename Enable = void> struct type_count
        {
                static const int value {0};
        };

        template <typename T>
        struct type_count<T,
                          typename std::enable_if<!is_wrapper<T>::value && !is_tuple_like<T>::value &&
                                                  !is_complex<T>::value && !std::is_void<T>::value>::type>
        {
                static constexpr int value {1};
        };

        template <typename T> struct type_count<T, typename std::enable_if<is_complex<T>::value>::type>
        {
                static constexpr int value {2};
        };

        template <typename T> struct type_count<T, typename std::enable_if<is_mutable_container<T>::value>::type>
        {
                static constexpr int value {subtype_count<typename T::value_type>::value};
        };

        template <typename T>
        struct type_count<T,
                          typename std::enable_if<is_wrapper<T>::value && !is_complex<T>::value &&
                                                  !is_tuple_like<T>::value && !is_mutable_container<T>::value>::type>
        {
                static constexpr int value {type_count<typename T::value_type>::value};
        };

        template <typename T, std::size_t I>
        constexpr typename std::enable_if<I == type_count_base<T>::value, int>::type tuple_type_size()
        {
            return 0;
        }

        template <typename T, std::size_t I>
            constexpr typename std::enable_if < I<type_count_base<T>::value, int>::type tuple_type_size()
        {
            return subtype_count<typename std::tuple_element<I, T>::type>::value + tuple_type_size<T, I + 1>();
        }

        template <typename T>
        struct type_count<T, typename std::enable_if<is_tuple_like<T>::value && !is_complex<T>::value>::type>
        {
                static constexpr int value {tuple_type_size<T, 0>()};
        };

        template <typename T> struct subtype_count
        {
                static constexpr int value {is_mutable_container<T>::value ? expected_max_vector_size
                                                                           : type_count<T>::value};
        };

        template <typename T, typename Enable = void> struct type_count_min
        {
                static const int value {0};
        };

        template <typename T>
        struct type_count_min<
            T,
            typename std::enable_if<!is_mutable_container<T>::value && !is_tuple_like<T>::value &&
                                    !is_wrapper<T>::value && !is_complex<T>::value && !std::is_void<T>::value>::type>
        {
                static constexpr int value {type_count<T>::value};
        };

        template <typename T> struct type_count_min<T, typename std::enable_if<is_complex<T>::value>::type>
        {
                static constexpr int value {1};
        };

        template <typename T>
        struct type_count_min<
            T,
            typename std::enable_if<is_wrapper<T>::value && !is_complex<T>::value && !is_tuple_like<T>::value>::type>
        {
                static constexpr int value {subtype_count_min<typename T::value_type>::value};
        };

        template <typename T, std::size_t I>
        constexpr typename std::enable_if<I == type_count_base<T>::value, int>::type tuple_type_size_min()
        {
            return 0;
        }

        template <typename T, std::size_t I>
            constexpr typename std::enable_if < I<type_count_base<T>::value, int>::type tuple_type_size_min()
        {
            return subtype_count_min<typename std::tuple_element<I, T>::type>::value + tuple_type_size_min<T, I + 1>();
        }

        template <typename T>
        struct type_count_min<T, typename std::enable_if<is_tuple_like<T>::value && !is_complex<T>::value>::type>
        {
                static constexpr int value {tuple_type_size_min<T, 0>()};
        };

        template <typename T> struct subtype_count_min
        {
                static constexpr int value {
                    is_mutable_container<T>::value
                        ? ((type_count<T>::value < expected_max_vector_size) ? type_count<T>::value : 0)
                        : type_count_min<T>::value};
        };

        template <typename T, typename Enable = void> struct expected_count
        {
                static const int value {0};
        };

        template <typename T>
        struct expected_count<T,
                              typename std::enable_if<!is_mutable_container<T>::value && !is_wrapper<T>::value &&
                                                      !std::is_void<T>::value>::type>
        {
                static constexpr int value {1};
        };

        template <typename T> struct expected_count<T, typename std::enable_if<is_mutable_container<T>::value>::type>
        {
                static constexpr int value {expected_max_vector_size};
        };

        template <typename T>
        struct expected_count<T, typename std::enable_if<!is_mutable_container<T>::value && is_wrapper<T>::value>::type>
        {
                static constexpr int value {expected_count<typename T::value_type>::value};
        };

        enum class object_category : std::uint8_t
        {
            char_value = 1,
            integral_value = 2,
            unsigned_integral = 4,
            enumeration = 6,
            boolean_value = 8,
            floating_point = 10,
            number_constructible = 12,
            double_constructible = 14,
            integer_constructible = 16,
            string_assignable = 23,
            string_constructible = 24,
            wstring_assignable = 25,
            wstring_constructible = 26,
            other = 45,
            wrapper_value = 50,
            complex_number = 60,
            tuple_value = 70,
            container_value = 80,
        };

        template <typename T, typename Enable = void> struct classify_object
        {
                static constexpr object_category value {object_category::other};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value &&
                                    std::is_signed<T>::value && !is_bool<T>::value && !std::is_enum<T>::value>::type>
        {
                static constexpr object_category value {object_category::integral_value};
        };

        template <typename T>
        struct classify_object<T,
                               typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value &&
                                                       !std::is_same<T, char>::value && !is_bool<T>::value>::type>
        {
                static constexpr object_category value {object_category::unsigned_integral};
        };

        template <typename T>
        struct classify_object<T,
                               typename std::enable_if<std::is_same<T, char>::value && !std::is_enum<T>::value>::type>
        {
                static constexpr object_category value {object_category::char_value};
        };

        template <typename T> struct classify_object<T, typename std::enable_if<is_bool<T>::value>::type>
        {
                static constexpr object_category value {object_category::boolean_value};
        };

        template <typename T> struct classify_object<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
        {
                static constexpr object_category value {object_category::floating_point};
        };

#if defined _MSC_VER
#define WIDE_STRING_CHECK                                                                                              \
    !std::is_assignable<T &, std::wstring>::value && !std::is_constructible<T, std::wstring>::value
#define STRING_CHECK true
#else
#define WIDE_STRING_CHECK true
#define STRING_CHECK !std::is_assignable<T &, std::string>::value && !std::is_constructible<T, std::string>::value
#endif

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                                    WIDE_STRING_CHECK && std::is_assignable<T &, std::string>::value>::type>
        {
                static constexpr object_category value {object_category::string_assignable};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                                    !std::is_assignable<T &, std::string>::value && (type_count<T>::value == 1) &&
                                    WIDE_STRING_CHECK && std::is_constructible<T, std::string>::value>::type>
        {
                static constexpr object_category value {object_category::string_constructible};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value && STRING_CHECK &&
                                    std::is_assignable<T &, std::wstring>::value>::type>
        {
                static constexpr object_category value {object_category::wstring_assignable};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                                    !std::is_assignable<T &, std::wstring>::value && (type_count<T>::value == 1) &&
                                    STRING_CHECK && std::is_constructible<T, std::wstring>::value>::type>
        {
                static constexpr object_category value {object_category::wstring_constructible};
        };

#undef WIDE_STRING_CHECK
#undef STRING_CHECK

        template <typename T> struct classify_object<T, typename std::enable_if<std::is_enum<T>::value>::type>
        {
                static constexpr object_category value {object_category::enumeration};
        };

        template <typename T> struct classify_object<T, typename std::enable_if<is_complex<T>::value>::type>
        {
                static constexpr object_category value {object_category::complex_number};
        };

        template <typename T> struct uncommon_type
        {
                using type = typename std::conditional<
                    !std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                        !std::is_assignable<T &, std::string>::value && !std::is_constructible<T, std::string>::value &&
                        !std::is_assignable<T &, std::wstring>::value &&
                        !std::is_constructible<T, std::wstring>::value && !is_complex<T>::value &&
                        !is_mutable_container<T>::value && !std::is_enum<T>::value,
                    std::true_type,
                    std::false_type>::type;
                static constexpr bool value = type::value;
        };

        template <typename T>
        struct classify_object<T,
                               typename std::enable_if<(!is_mutable_container<T>::value && is_wrapper<T>::value &&
                                                        !is_tuple_like<T>::value && uncommon_type<T>::value)>::type>
        {
                static constexpr object_category value {object_category::wrapper_value};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 && !is_wrapper<T>::value &&
                                    is_direct_constructible<T, double>::value &&
                                    is_direct_constructible<T, int>::value>::type>
        {
                static constexpr object_category value {object_category::number_constructible};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 && !is_wrapper<T>::value &&
                                    !is_direct_constructible<T, double>::value &&
                                    is_direct_constructible<T, int>::value>::type>
        {
                static constexpr object_category value {object_category::integer_constructible};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 && !is_wrapper<T>::value &&
                                    is_direct_constructible<T, double>::value &&
                                    !is_direct_constructible<T, int>::value>::type>
        {
                static constexpr object_category value {object_category::double_constructible};
        };

        template <typename T>
        struct classify_object<
            T,
            typename std::enable_if<is_tuple_like<T>::value &&
                                    ((type_count<T>::value >= 2 && !is_wrapper<T>::value) ||
                                     (uncommon_type<T>::value && !is_direct_constructible<T, double>::value &&
                                      !is_direct_constructible<T, int>::value) ||
                                     (uncommon_type<T>::value && type_count<T>::value >= 2))>::type>
        {
                static constexpr object_category value {object_category::tuple_value};
        };

        template <typename T> struct classify_object<T, typename std::enable_if<is_mutable_container<T>::value>::type>
        {
                static constexpr object_category value {object_category::container_value};
        };

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::char_value, detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "CHAR";
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::integral_value ||
                                  classify_object<T>::value == object_category::integer_constructible,
                              detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "INT";
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::unsigned_integral, detail::enabler> =
                      detail::dummy>
        constexpr const char *type_name()
        {
            return "UINT";
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::floating_point ||
                                  classify_object<T>::value == object_category::number_constructible ||
                                  classify_object<T>::value == object_category::double_constructible,
                              detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "FLOAT";
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::enumeration, detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "ENUM";
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::boolean_value, detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "BOOLEAN";
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::complex_number, detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "COMPLEX";
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value >= object_category::string_assignable &&
                                  classify_object<T>::value <= object_category::other,
                              detail::enabler> = detail::dummy>
        constexpr const char *type_name()
        {
            return "TEXT";
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value >= 2,
                        detail::enabler> = detail::dummy>
        std::string type_name();

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::container_value ||
                                  classify_object<T>::value == object_category::wrapper_value,
                              detail::enabler> = detail::dummy>
        std::string type_name();

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value == 1,
                        detail::enabler> = detail::dummy>
        std::string type_name()
        {
            return type_name<typename std::decay<typename std::tuple_element<0, T>::type>::type>();
        }

        template <typename T, std::size_t I>
        typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_name()
        {
            return std::string {};
        }

        template <typename T, std::size_t I>
        typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_name()
        {
            auto str = std::string {type_name<typename std::decay<typename std::tuple_element<I, T>::type>::type>()} +
                       ',' + tuple_name<T, I + 1>();
            if (str.back() == ',')
                str.pop_back();
            return str;
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value >= 2,
                        detail::enabler>>
        std::string type_name()
        {
            auto tname = std::string(1, '[') + tuple_name<T, 0>();
            tname.push_back(']');
            return tname;
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::container_value ||
                                  classify_object<T>::value == object_category::wrapper_value,
                              detail::enabler>>
        std::string type_name()
        {
            return type_name<typename T::value_type>();
        }

        template <typename T, enable_if_t<std::is_unsigned<T>::value, detail::enabler> = detail::dummy>
        bool integral_conversion(const std::string &input, T &output) noexcept
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
            std::int64_t output_sll = std::strtoll(input.c_str(), &val, 0);
            if (val == (input.c_str() + input.size()))
            {
                output = (output_sll < 0) ? static_cast<T>(0) : static_cast<T>(output_sll);
                return (static_cast<std::int64_t>(output) == output_sll);
            }
            auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                std::string nstring = input;
                for (auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        nstring.erase(std::remove(nstring.begin(), nstring.end(), separator), nstring.end());
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

        template <typename T, enable_if_t<std::is_signed<T>::value, detail::enabler> = detail::dummy>
        bool integral_conversion(const std::string &input, T &output) noexcept
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
            auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                for (auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        std::string nstring = input;
                        nstring.erase(std::remove(nstring.begin(), nstring.end(), separator), nstring.end());
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

        std::int64_t to_flag_value(std::string val) noexcept
        {
            static const std::string trueString("true");
            static const std::string falseString("false");
            if (val == trueString)
            {
                return 1;
            }
            if (val == falseString)
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
            if (val == trueString || val == "on" || val == "yes" || val == "enable")
            {
                ret = 1;
            }
            else if (val == falseString || val == "off" || val == "no" || val == "disable")
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

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::integral_value ||
                                  classify_object<T>::value == object_category::unsigned_integral,
                              detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            return integral_conversion(input, output);
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::char_value, detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            if (input.size() == 1)
            {
                output = static_cast<T>(input[0]);
                return true;
            }
            std::int8_t res {0};
            bool result = integral_conversion(input, res);
            if (result)
            {
                output = static_cast<T>(res);
            }
            return result;
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::boolean_value, detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            errno = 0;
            auto out = to_flag_value(input);
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

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::floating_point, detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            if (input.empty())
            {
                return false;
            }
            char *val = nullptr;
            auto output_ld = std::strtold(input.c_str(), &val);
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

            auto group_separators = get_group_separators();
            if (input.find_first_of(group_separators) != std::string::npos)
            {
                for (auto &separator : group_separators)
                {
                    if (input.find_first_of(separator) != std::string::npos)
                    {
                        std::string nstring = input;
                        nstring.erase(std::remove(nstring.begin(), nstring.end(), separator), nstring.end());
                        return lexical_cast(nstring, output);
                    }
                }
            }
            return false;
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::complex_number, detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            using XC = typename wrapped_type<T, double>::type;
            XC x {0.0}, y {0.0};
            auto str1 = input;
            bool worked = false;
            auto nloc = str1.find_last_of("+-");
            if (nloc != std::string::npos && nloc > 0)
            {
                worked = lexical_cast(str1.substr(0, nloc), x);
                str1 = str1.substr(nloc);
                if (str1.back() == 'i' || str1.back() == 'j')
                    str1.pop_back();
                worked = worked && lexical_cast(str1, y);
            }
            else
            {
                if (str1.back() == 'i' || str1.back() == 'j')
                {
                    str1.pop_back();
                    worked = lexical_cast(str1, y);
                    x = XC {0};
                }
                else
                {
                    worked = lexical_cast(str1, x);
                    y = XC {0};
                }
            }
            if (worked)
            {
                output = T {x, y};
                return worked;
            }
            return from_stream(input, output);
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::string_assignable, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            output = input;
            return true;
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::string_constructible, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            output = T(input);
            return true;
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::wstring_assignable, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            output = widen(input);
            return true;
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::wstring_constructible, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            output = T {widen(input)};
            return true;
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::enumeration, detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            typename std::underlying_type<T>::type val;
            if (!integral_conversion(input, val))
            {
                return false;
            }
            output = static_cast<T>(val);
            return true;
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::wrapper_value &&
                                  std::is_assignable<T &, typename T::value_type>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            typename T::value_type val;
            if (lexical_cast(input, val))
            {
                output = val;
                return true;
            }
            return from_stream(input, output);
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::wrapper_value &&
                                  !std::is_assignable<T &, typename T::value_type>::value &&
                                  std::is_assignable<T &, T>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            typename T::value_type val;
            if (lexical_cast(input, val))
            {
                output = T {val};
                return true;
            }
            return from_stream(input, output);
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::number_constructible, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
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

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::integer_constructible, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            int val = 0;
            if (integral_conversion(input, val))
            {
                output = T(val);
                return true;
            }
            return from_stream(input, output);
        }

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::double_constructible, detail::enabler> =
                      detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            double val = 0.0;
            if (lexical_cast(input, val))
            {
                output = T {val};
                return true;
            }
            return from_stream(input, output);
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::other && std::is_assignable<T &, int>::value,
                        detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
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

        template <typename T,
                  enable_if_t<classify_object<T>::value == object_category::other &&
                                  !std::is_assignable<T &, int>::value && is_istreamable<T>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string &input, T &output)
        {
            return from_stream(input, output);
        }

        template <
            typename T,
            enable_if_t<classify_object<T>::value == object_category::other && !std::is_assignable<T &, int>::value &&
                            !is_istreamable<T>::value && !adl_detail::is_lexical_castable<T>::value,
                        detail::enabler> = detail::dummy>
        bool lexical_cast(const std::string & /*input*/, T & /*output*/)
        {
            static_assert(
                !std::is_same<T, T>::value,
                "option object type must have a lexical cast overload or streaming input operator(>>) defined, if it "
                "is convertible from another type use the add_option<T, XC>(...) with XC being the known type");
            return false;
        }

        template <typename AssignTo,
                  typename ConvertTo,
                  enable_if_t<std::is_same<AssignTo, ConvertTo>::value &&
                                  (classify_object<AssignTo>::value == object_category::string_assignable ||
                                   classify_object<AssignTo>::value == object_category::string_constructible ||
                                   classify_object<AssignTo>::value == object_category::wstring_assignable ||
                                   classify_object<AssignTo>::value == object_category::wstring_constructible),
                              detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
        {
            return lexical_cast(input, output);
        }

        template <
            typename AssignTo,
            typename ConvertTo,
            enable_if_t<std::is_same<AssignTo, ConvertTo>::value && std::is_assignable<AssignTo &, AssignTo>::value &&
                            classify_object<AssignTo>::value != object_category::string_assignable &&
                            classify_object<AssignTo>::value != object_category::string_constructible &&
                            classify_object<AssignTo>::value != object_category::wstring_assignable &&
                            classify_object<AssignTo>::value != object_category::wstring_constructible,
                        detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
        {
            if (input.empty())
            {
                output = AssignTo {};
                return true;
            }

            return lexical_cast(input, output);
        }

        template <
            typename AssignTo,
            typename ConvertTo,
            enable_if_t<std::is_same<AssignTo, ConvertTo>::value && !std::is_assignable<AssignTo &, AssignTo>::value &&
                            classify_object<AssignTo>::value == object_category::wrapper_value,
                        detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
        {
            if (input.empty())
            {
                typename AssignTo::value_type emptyVal {};
                output = emptyVal;
                return true;
            }
            return lexical_cast(input, output);
        }

        template <
            typename AssignTo,
            typename ConvertTo,
            enable_if_t<std::is_same<AssignTo, ConvertTo>::value && !std::is_assignable<AssignTo &, AssignTo>::value &&
                            classify_object<AssignTo>::value != object_category::wrapper_value &&
                            std::is_assignable<AssignTo &, int>::value,
                        detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
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

        template <
            typename AssignTo,
            typename ConvertTo,
            enable_if_t<!std::is_same<AssignTo, ConvertTo>::value && std::is_assignable<AssignTo &, ConvertTo &>::value,
                        detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
        {
            ConvertTo val {};
            bool parse_result = (!input.empty()) ? lexical_cast(input, val) : true;
            if (parse_result)
            {
                output = val;
            }
            return parse_result;
        }

        template <typename AssignTo,
                  typename ConvertTo,
                  enable_if_t<!std::is_same<AssignTo, ConvertTo>::value &&
                                  !std::is_assignable<AssignTo &, ConvertTo &>::value &&
                                  std::is_move_assignable<AssignTo>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_assign(const std::string &input, AssignTo &output)
        {
            ConvertTo val {};
            bool parse_result = input.empty() ? true : lexical_cast(input, val);
            if (parse_result)
            {
                output = AssignTo(val);
            }
            return parse_result;
        }

        template <typename AssignTo,
                  typename ConvertTo,
                  enable_if_t<classify_object<ConvertTo>::value <= object_category::other &&
                                  classify_object<AssignTo>::value <= object_category::wrapper_value,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {
            return lexical_assign<AssignTo, ConvertTo>(strings[0], output);
        }

        template <typename AssignTo,
                  typename ConvertTo,
                  enable_if_t<(type_count<AssignTo>::value <= 2) && expected_count<AssignTo>::value == 1 &&
                                  is_tuple_like<ConvertTo>::value && type_count_base<ConvertTo>::value == 2,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {
            using FirstType = typename std::remove_const<typename std::tuple_element<0, ConvertTo>::type>::type;
            using SecondType = typename std::tuple_element<1, ConvertTo>::type;
            FirstType v1;
            SecondType v2 {};
            bool retval = lexical_assign<FirstType, FirstType>(strings[0], v1);
            retval = retval &&
                     lexical_assign<SecondType, SecondType>((strings.size() > 1) ? strings[1] : std::string {}, v2);
            if (retval)
            {
                output = AssignTo {v1, v2};
            }
            return retval;
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                                  type_count<ConvertTo>::value == 1,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
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
                typename AssignTo::value_type out;
                bool retval = lexical_assign<typename AssignTo::value_type, typename ConvertTo::value_type>(elem, out);
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

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_complex<ConvertTo>::value, detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output)
        {

            if (strings.size() >= 2 && !strings[1].empty())
            {
                using XC2 = typename wrapped_type<ConvertTo, double>::type;
                XC2 x {0.0}, y {0.0};
                auto str1 = strings[1];
                if (str1.back() == 'i' || str1.back() == 'j')
                {
                    str1.pop_back();
                }
                auto worked = lexical_cast(strings[0], x) && lexical_cast(str1, y);
                if (worked)
                {
                    output = ConvertTo {x, y};
                }
                return worked;
            }
            return lexical_assign<AssignTo, ConvertTo>(strings[0], output);
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && (expected_count<ConvertTo>::value == 1) &&
                                  (type_count<ConvertTo>::value == 1),
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {
            bool retval = true;
            output.clear();
            output.reserve(strings.size());
            for (const auto &elem : strings)
            {

                output.emplace_back();
                retval = retval && lexical_assign<typename AssignTo::value_type, ConvertTo>(elem, output.back());
            }
            return (!output.empty()) && retval;
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                                  type_count_base<ConvertTo>::value == 2,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(std::vector<std::string> strings, AssignTo &output);

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                                  type_count_base<ConvertTo>::value != 2 &&
                                  ((type_count<ConvertTo>::value > 2) ||
                                   (type_count<ConvertTo>::value > type_count_base<ConvertTo>::value)),
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output);

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_tuple_like<AssignTo>::value && is_tuple_like<ConvertTo>::value &&
                                  (type_count_base<ConvertTo>::value != type_count<ConvertTo>::value ||
                                   type_count<ConvertTo>::value > 2),
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output);

        template <typename AssignTo,
                  typename ConvertTo,
                  enable_if_t<!is_tuple_like<AssignTo>::value && !is_mutable_container<AssignTo>::value &&
                                  classify_object<ConvertTo>::value != object_category::wrapper_value &&
                                  (is_mutable_container<ConvertTo>::value || type_count<ConvertTo>::value > 2),
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {

            if (strings.size() > 1 || (!strings.empty() && !(strings.front().empty())))
            {
                ConvertTo val;
                auto retval = lexical_conversion<ConvertTo, ConvertTo>(strings, val);
                output = AssignTo {val};
                return retval;
            }
            output = AssignTo {};
            return true;
        }

        template <class AssignTo, class ConvertTo, std::size_t I>
        typename std::enable_if<(I >= type_count_base<AssignTo>::value), bool>::type tuple_conversion(
            const std::vector<std::string> &, AssignTo &)
        {
            return true;
        }

        template <class AssignTo, class ConvertTo>
        typename std::enable_if<!is_mutable_container<ConvertTo>::value && type_count<ConvertTo>::value == 1,
                                bool>::type
        tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output)
        {
            auto retval = lexical_assign<AssignTo, ConvertTo>(strings[0], output);
            strings.erase(strings.begin());
            return retval;
        }

        template <class AssignTo, class ConvertTo>
        typename std::enable_if<!is_mutable_container<ConvertTo>::value && (type_count<ConvertTo>::value > 1) &&
                                    type_count<ConvertTo>::value == type_count_min<ConvertTo>::value,
                                bool>::type
        tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output)
        {
            auto retval = lexical_conversion<AssignTo, ConvertTo>(strings, output);
            strings.erase(strings.begin(), strings.begin() + type_count<ConvertTo>::value);
            return retval;
        }

        template <class AssignTo, class ConvertTo>
        typename std::enable_if<is_mutable_container<ConvertTo>::value ||
                                    type_count<ConvertTo>::value != type_count_min<ConvertTo>::value,
                                bool>::type
        tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output)
        {

            std::size_t index {subtype_count_min<ConvertTo>::value};
            const std::size_t mx_count {subtype_count<ConvertTo>::value};
            const std::size_t mx {(std::min)(mx_count, strings.size() - 1)};

            while (index < mx)
            {
                if (is_separator(strings[index]))
                {
                    break;
                }
                ++index;
            }
            bool retval = lexical_conversion<AssignTo, ConvertTo>(
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

        template <class AssignTo, class ConvertTo, std::size_t I>
        typename std::enable_if<(I < type_count_base<AssignTo>::value), bool>::type tuple_conversion(
            std::vector<std::string> strings, AssignTo &output)
        {
            bool retval = true;
            using ConvertToElement = typename std::conditional<is_tuple_like<ConvertTo>::value,
                                                               typename std::tuple_element<I, ConvertTo>::type,
                                                               ConvertTo>::type;
            if (!strings.empty())
            {
                retval =
                    retval && tuple_type_conversion<typename std::tuple_element<I, AssignTo>::type, ConvertToElement>(
                                  strings, std::get<I>(output));
            }
            retval = retval && tuple_conversion<AssignTo, ConvertTo, I + 1>(std::move(strings), output);
            return retval;
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                                  type_count_base<ConvertTo>::value == 2,
                              detail::enabler>>
        bool lexical_conversion(std::vector<std::string> strings, AssignTo &output)
        {
            output.clear();
            while (!strings.empty())
            {

                typename std::remove_const<typename std::tuple_element<0, typename ConvertTo::value_type>::type>::type
                    v1;
                typename std::tuple_element<1, typename ConvertTo::value_type>::type v2;
                bool retval = tuple_type_conversion<decltype(v1), decltype(v1)>(strings, v1);
                if (!strings.empty())
                {
                    retval = retval && tuple_type_conversion<decltype(v2), decltype(v2)>(strings, v2);
                }
                if (retval)
                {
                    output.insert(output.end(), typename AssignTo::value_type {v1, v2});
                }
                else
                {
                    return false;
                }
            }
            return (!output.empty());
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_tuple_like<AssignTo>::value && is_tuple_like<ConvertTo>::value &&
                                  (type_count_base<ConvertTo>::value != type_count<ConvertTo>::value ||
                                   type_count<ConvertTo>::value > 2),
                              detail::enabler>>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {
            static_assert(!is_tuple_like<ConvertTo>::value ||
                              type_count_base<AssignTo>::value == type_count_base<ConvertTo>::value,
                          "if the conversion type is defined as a tuple it must be the same size as the type you are "
                          "converting to");
            return tuple_conversion<AssignTo, ConvertTo, 0>(strings, output);
        }

        template <class AssignTo,
                  class ConvertTo,
                  enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                                  type_count_base<ConvertTo>::value != 2 &&
                                  ((type_count<ConvertTo>::value > 2) ||
                                   (type_count<ConvertTo>::value > type_count_base<ConvertTo>::value)),
                              detail::enabler>>
        bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output)
        {
            bool retval = true;
            output.clear();
            std::vector<std::string> temp;
            std::size_t ii {0};
            std::size_t icount {0};
            std::size_t xcm {type_count<ConvertTo>::value};
            auto ii_max = strings.size();
            while (ii < ii_max)
            {
                temp.push_back(strings[ii]);
                ++ii;
                ++icount;
                if (icount == xcm || is_separator(temp.back()) || ii == ii_max)
                {
                    if (static_cast<int>(xcm) > type_count_min<ConvertTo>::value && is_separator(temp.back()))
                    {
                        temp.pop_back();
                    }
                    typename AssignTo::value_type temp_out;
                    retval =
                        retval && lexical_conversion<typename AssignTo::value_type, typename ConvertTo::value_type>(
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

        template <typename AssignTo,
                  class ConvertTo,
                  enable_if_t<classify_object<ConvertTo>::value == object_category::wrapper_value &&
                                  std::is_assignable<ConvertTo &, ConvertTo>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output)
        {
            if (strings.empty() || strings.front().empty())
            {
                output = ConvertTo {};
                return true;
            }
            typename ConvertTo::value_type val;
            if (lexical_conversion<typename ConvertTo::value_type, typename ConvertTo::value_type>(strings, val))
            {
                output = ConvertTo {val};
                return true;
            }
            return false;
        }

        template <typename AssignTo,
                  class ConvertTo,
                  enable_if_t<classify_object<ConvertTo>::value == object_category::wrapper_value &&
                                  !std::is_assignable<AssignTo &, ConvertTo>::value,
                              detail::enabler> = detail::dummy>
        bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output)
        {
            using ConvertType = typename ConvertTo::value_type;
            if (strings.empty() || strings.front().empty())
            {
                output = ConvertType {};
                return true;
            }
            ConvertType val;
            if (lexical_conversion<typename ConvertTo::value_type, typename ConvertTo::value_type>(strings, val))
            {
                output = val;
                return true;
            }
            return false;
        }

        std::string sum_string_vector(const std::vector<std::string> &values)
        {
            double val {0.0};
            bool fail {false};
            std::string output;
            for (const auto &arg : values)
            {
                double tv {0.0};
                auto comp = lexical_cast(arg, tv);
                if (!comp)
                {
                    errno = 0;
                    auto fv = to_flag_value(arg);
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
                std::ostringstream out;
                out.precision(16);
                out << val;
                output = out.str();
            }
            return output;
        }

    } // namespace detail
} // namespace CLI
