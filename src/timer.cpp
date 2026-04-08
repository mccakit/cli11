module;
import std;

export module cli11:timer;

export namespace CLI
{

    class Timer
    {
        protected:
            using clock = std::chrono::steady_clock;
            using time_point = std::chrono::time_point<clock>;
            using time_print_t = std::function<std::string(std::string, std::string)>;

            std::string title_;
            time_print_t time_print_;
            time_point start_;
            std::size_t cycles {1};

        public:
            static std::string Simple(std::string title, std::string time)
            {
                return title + ": " + time;
            }

            static std::string Big(std::string title, std::string time)
            {
                return std::string("-----------------------------------------\n") + "| " + title + " | Time = " + time +
                       "\n" + "-----------------------------------------";
            }

            explicit Timer(std::string title = "Timer", time_print_t time_print = Simple)
                : title_(std::move(title)), time_print_(std::move(time_print)), start_(clock::now())
            {
            }

            std::string time_it(std::function<void()> f, double target_time = 1)
            {
                time_point start = start_;
                double total_time = std::numeric_limits<double>::quiet_NaN();
                start_ = clock::now();
                std::size_t n = 0;
                do
                {
                    f();
                    std::chrono::duration<double> elapsed = clock::now() - start_;
                    total_time = elapsed.count();
                } while (n++ < 100u && total_time < target_time);
                std::string out =
                    make_time_str(total_time / static_cast<double>(n)) + " for " + std::to_string(n) + " tries";
                start_ = start;
                return out;
            }

            [[nodiscard]] std::string make_time_str() const
            {
                time_point stop = clock::now();
                std::chrono::duration<double> elapsed = stop - start_;
                double time = elapsed.count() / static_cast<double>(cycles);
                return make_time_str(time);
            }

            [[nodiscard]] std::string make_time_str(double time) const
            {
                auto print_it = [](double x, std::string unit) {
                    const unsigned int buffer_length = 50;
                    std::array<char, buffer_length> buffer;
                    std::snprintf(buffer.data(), buffer_length, "%.5g", x);
                    return buffer.data() + std::string(" ") + unit;
                };
                if (time < .000001)
                    return print_it(time * 1000000000, "ns");
                if (time < .001)
                    return print_it(time * 1000000, "us");
                if (time < 1)
                    return print_it(time * 1000, "ms");
                return print_it(time, "s");
            }

            [[nodiscard]] std::string to_string() const
            {
                return time_print_(title_, make_time_str());
            }

            Timer &operator/(std::size_t val)
            {
                cycles = val;
                return *this;
            }
    };

    class AutoTimer : public Timer
    {
        public:
            explicit AutoTimer(std::string title = "Timer", time_print_t time_print = Simple) : Timer(title, time_print)
            {
            }
            ~AutoTimer()
            {
                std::cout << to_string() << '\n';
            }
    };

} // namespace CLI

export std::ostream &operator<<(std::ostream &in, const CLI::Timer &timer)
{
    return in << timer.to_string();
}
