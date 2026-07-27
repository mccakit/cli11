/// @file
/// @brief Small wall-clock timing helpers.
///
/// @ref cli::timer_t measures elapsed time from its own construction and renders
/// it through a caller-supplied formatting callable. @ref cli::auto_timer_t is the
/// same thing with a destructor that reports automatically, so timing a scope is
/// a single declaration:
///
/// @code
/// {
///     const cli::auto_timer_t t{"parse"};
///     app.parse(argc, argv);
/// } // prints "parse: 1.2345 ms"
/// @endcode

export module cli11:timer;

import std;

export namespace cli
{

    /// @brief Measures elapsed wall-clock time from construction.
    ///
    /// The timer starts when it is constructed. @ref to_string renders the time
    /// elapsed since then, divided by the cycle count set through @ref operator/.
    class timer_t
    {
        protected:
            /// @brief The clock used for all measurements.
            ///
            /// @note This member alias shadows `std::clock_t` inside the class body.
            /// Nothing here refers to the C library type, so the shadowing is harmless,
            /// but qualify as `std::clock_t` if you ever need it.
            using clock_t = std::chrono::steady_clock;

            /// @brief A point in time as reported by @ref clock_t.
            using time_point_t = std::chrono::time_point<clock_t>;

            /// @brief Renders a finished measurement given a title and a formatted duration.
            using time_print_t = std::function<std::string(std::string, std::string)>;

            /// @brief Label reported alongside the measurement.
            std::string title_;

            /// @brief Callable used to render the finished measurement.
            time_print_t time_print_;

            /// @brief The instant this timer started measuring.
            time_point_t start_;

            /// @brief Number of cycles the measured interval is divided by.
            std::size_t cycles_ {1};

        public:
            /// @brief Renders a measurement as `"title: time"`.
            ///
            /// @param title Label for the measurement.
            /// @param time Preformatted duration.
            /// @return The rendered single-line measurement.
            [[nodiscard]] static auto simple(std::string title, std::string time) -> std::string
            {
                return title + ": " + time;
            }

            /// @brief Renders a measurement inside a full-width banner.
            ///
            /// @param title Label for the measurement.
            /// @param time Preformatted duration.
            /// @return The rendered multi-line measurement.
            [[nodiscard]] static auto big(std::string title, std::string time) -> std::string
            {
                return "-----------------------------------------\n| " + title + " | Time = " + time +
                       "\n-----------------------------------------";
            }

            /// @brief Starts a timer.
            ///
            /// @param title Label reported alongside the measurement.
            /// @param time_print Callable used to render the finished measurement.
            explicit timer_t(std::string title = "Timer", time_print_t time_print = simple)
                : title_(std::move(title)), time_print_(std::move(time_print)), start_(clock_t::now())
            {
            }

            /// @brief Repeatedly invokes @p f until @p target_time has elapsed.
            ///
            /// Runs @p f at least once and at most 100 times, stopping early once the
            /// accumulated time reaches @p target_time. The timer's own start point is
            /// restored before returning, so calling this does not disturb @ref to_string.
            ///
            /// @tparam F Any callable invocable with no arguments.
            /// @param f The callable to time.
            /// @param target_time Seconds to keep repeating for.
            /// @return A description of the mean time per call and the number of calls.
            template <std::invocable F> [[nodiscard]] auto time_it(F &&f, double target_time = 1.0) -> std::string
            {
                const time_point_t start = start_;
                double total_time = std::numeric_limits<double>::quiet_NaN();

                start_ = clock_t::now();
                std::size_t n = 0;
                do
                {
                    std::invoke(f);
                    const std::chrono::duration<double> elapsed = clock_t::now() - start_;
                    total_time = elapsed.count();
                } while (n++ < 100U && total_time < target_time);

                std::string out =
                    make_time_str(total_time / static_cast<double>(n)) + " for " + std::to_string(n) + " tries";
                start_ = start;
                return out;
            }

            /// @brief Formats the time elapsed since construction, divided by the cycle count.
            ///
            /// @return The elapsed time with an automatically chosen unit.
            [[nodiscard]] auto make_time_str() const -> std::string
            {
                const std::chrono::duration<double> elapsed = clock_t::now() - start_;
                return make_time_str(elapsed.count() / static_cast<double>(cycles_));
            }

            /// @brief Formats a duration, choosing a unit that keeps the value readable.
            ///
            /// @param time A duration in seconds.
            /// @return The duration rendered to five significant digits, with a unit.
            [[nodiscard]] auto make_time_str(double time) const -> std::string
            {
                const auto print_it = [](double value, std::string_view unit) -> std::string {
                    return std::format("{:.5g} {}", value, unit);
                };

                if (time < .000001)
                {
                    return print_it(time * 1000000000, "ns");
                }
                if (time < .001)
                {
                    return print_it(time * 1000000, "us");
                }
                if (time < 1)
                {
                    return print_it(time * 1000, "ms");
                }
                return print_it(time, "s");
            }

            /// @brief Renders the measurement through the configured print callable.
            ///
            /// @return The fully rendered measurement.
            [[nodiscard]] auto to_string() const -> std::string
            {
                return time_print_(title_, make_time_str());
            }

            /// @brief Sets the divisor applied to the measured interval.
            ///
            /// @param val Number of cycles the interval represents.
            /// @return A reference to this timer, for chaining.
            auto operator/(std::size_t val) -> timer_t &
            {
                cycles_ = val;
                return *this;
            }
    };

    /// @brief A @ref timer_t that reports to `std::cout` when it goes out of scope.
    ///
    /// Copying or moving one of these would report the same interval more than once,
    /// so both are deleted.
    class auto_timer_t : public timer_t
    {
        public:
            /// @brief Starts a timer that reports on destruction.
            ///
            /// @param title Label reported alongside the measurement.
            /// @param time_print Callable used to render the finished measurement.
            explicit auto_timer_t(std::string title = "Timer", time_print_t time_print = simple)
                : timer_t(std::move(title), std::move(time_print))
            {
            }

            auto_timer_t(const auto_timer_t &) = delete;
            auto_timer_t(auto_timer_t &&) = delete;
            auto operator=(const auto_timer_t &) -> auto_timer_t & = delete;
            auto operator=(auto_timer_t &&) -> auto_timer_t & = delete;

            /// @brief Writes the measurement to `std::cout`.
            ~auto_timer_t()
            {
                std::cout << to_string() << '\n';
            }
    };

    /// @brief Streams a rendered measurement.
    ///
    /// @param in The stream to write to.
    /// @param timer The timer to render.
    /// @return @p in, for chaining.
    auto operator<<(std::ostream &in, const timer_t &timer) -> std::ostream &
    {
        return in << timer.to_string();
    }

} // namespace cli
