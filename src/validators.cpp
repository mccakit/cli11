// src/modules/validators.cppm
export module cli11:validators;

import std;
import :error;
import :string_tools;
import :type_tools;
import :encoding;

export namespace CLI {

class Option;

class Validator {
  protected:
    std::function<std::string()> desc_function_{[]() { return std::string{}; }};
    std::function<std::string(std::string &)> func_{[](std::string &) { return std::string{}; }};
    std::string name_{};
    int application_index_ = -1;
    bool active_{true};
    bool non_modifying_{false};

    Validator(std::string validator_desc, std::function<std::string(std::string &)> func)
        : desc_function_([validator_desc]() { return validator_desc; }), func_(std::move(func)) {}

  public:
    Validator() = default;

    explicit Validator(std::string validator_desc) : desc_function_([validator_desc]() { return validator_desc; }) {}

    Validator(std::function<std::string(std::string &)> op, std::string validator_desc,
              std::string validator_name = "")
        : desc_function_([validator_desc]() { return validator_desc; }), func_(std::move(op)),
          name_(std::move(validator_name)) {}

    Validator &operation(std::function<std::string(std::string &)> op) {
        func_ = std::move(op);
        return *this;
    }

    std::string operator()(std::string &str) const {
        std::string retstring;
        if (active_) {
            if (non_modifying_) {
                std::string value = str;
                retstring = func_(value);
            } else {
                retstring = func_(str);
            }
        }
        return retstring;
    }

    std::string operator()(const std::string &str) const {
        std::string value = str;
        return (active_) ? func_(value) : std::string{};
    }

    Validator &description(std::string validator_desc) {
        desc_function_ = [validator_desc]() { return validator_desc; };
        return *this;
    }

    [[nodiscard]] Validator description(std::string validator_desc) const {
        Validator newval(*this);
        newval.desc_function_ = [validator_desc]() { return validator_desc; };
        return newval;
    }

    [[nodiscard]] std::string get_description() const {
        if (active_) {
            return desc_function_();
        }
        return std::string{};
    }

    Validator &name(std::string validator_name) {
        name_ = std::move(validator_name);
        return *this;
    }

    [[nodiscard]] Validator name(std::string validator_name) const {
        Validator newval(*this);
        newval.name_ = std::move(validator_name);
        return newval;
    }

    [[nodiscard]] const std::string &get_name() const { return name_; }

    Validator &active(bool active_val = true) {
        active_ = active_val;
        return *this;
    }

    [[nodiscard]] Validator active(bool active_val = true) const {
        Validator newval(*this);
        newval.active_ = active_val;
        return newval;
    }

    [[nodiscard]] bool get_active() const { return active_; }

    Validator &non_modifying(bool no_modify = true) {
        non_modifying_ = no_modify;
        return *this;
    }

    [[nodiscard]] bool get_modifying() const { return !non_modifying_; }

    Validator &application_index(int app_index) {
        application_index_ = app_index;
        return *this;
    }

    [[nodiscard]] Validator application_index(int app_index) const {
        Validator newval(*this);
        newval.application_index_ = app_index;
        return newval;
    }

    [[nodiscard]] int get_application_index() const { return application_index_; }

    Validator operator&(const Validator &other) const {
        Validator newval;
        newval._merge_description(*this, other, " AND ");

        const std::function<std::string(std::string &)> &f1 = func_;
        const std::function<std::string(std::string &)> &f2 = other.func_;

        newval.func_ = [f1, f2](std::string &input) {
            std::string s1 = f1(input);
            std::string s2 = f2(input);
            if (!s1.empty() && !s2.empty())
                return std::string("(") + s1 + ") AND (" + s2 + ")";
            return s1 + s2;
        };

        newval.active_ = active_ && other.active_;
        newval.application_index_ = application_index_;
        return newval;
    }

    Validator operator|(const Validator &other) const {
        Validator newval;
        newval._merge_description(*this, other, " OR ");

        const std::function<std::string(std::string &)> &f1 = func_;
        const std::function<std::string(std::string &)> &f2 = other.func_;

        newval.func_ = [f1, f2](std::string &input) {
            std::string s1 = f1(input);
            std::string s2 = f2(input);
            if (s1.empty() || s2.empty())
                return std::string();
            return std::string("(") + s1 + ") OR (" + s2 + ")";
        };
        newval.active_ = active_ && other.active_;
        newval.application_index_ = application_index_;
        return newval;
    }

    Validator operator!() const {
        Validator newval;
        const std::function<std::string()> &dfunc1 = desc_function_;
        newval.desc_function_ = [dfunc1]() {
            auto str = dfunc1();
            return (!str.empty()) ? std::string("NOT ") + str : std::string{};
        };
        const std::function<std::string(std::string &)> &f1 = func_;

        newval.func_ = [f1, dfunc1](std::string &test) -> std::string {
            std::string s1 = f1(test);
            if (s1.empty()) {
                return std::string("check ") + dfunc1() + " succeeded improperly";
            }
            return std::string{};
        };
        newval.active_ = active_;
        newval.application_index_ = application_index_;
        return newval;
    }

  private:
    void _merge_description(const Validator &val1, const Validator &val2, const std::string &merger) {
        const std::function<std::string()> &dfunc1 = val1.desc_function_;
        const std::function<std::string()> &dfunc2 = val2.desc_function_;

        desc_function_ = [=]() {
            std::string f1 = dfunc1();
            std::string f2 = dfunc2();
            if ((f1.empty()) || (f2.empty())) {
                return f1 + f2;
            }
            return std::string(1, '(') + f1 + ')' + merger + '(' + f2 + ')';
        };
    }
};

using CustomValidator = Validator;

// --- Path checking ---

namespace detail {

enum class path_type : std::uint8_t { nonexistent, file, directory };

path_type check_path(const char *file) noexcept {
    std::error_code ec;
    auto stat = std::filesystem::status(to_path(file), ec);
    if (ec) {
        return path_type::nonexistent;
    }
    switch (stat.type()) {
    case std::filesystem::file_type::none:
    case std::filesystem::file_type::not_found:
        return path_type::nonexistent;
    case std::filesystem::file_type::directory:
        return path_type::directory;
    case std::filesystem::file_type::symlink:
    case std::filesystem::file_type::block:
    case std::filesystem::file_type::character:
    case std::filesystem::file_type::fifo:
    case std::filesystem::file_type::socket:
    case std::filesystem::file_type::regular:
    case std::filesystem::file_type::unknown:
    default:
        return path_type::file;
    }
}

}  // namespace detail

// --- Built-in Validators ---

inline const Validator ExistingFile{
    [](std::string &filename) {
        auto path_result = detail::check_path(filename.c_str());
        if (path_result == detail::path_type::nonexistent) {
            return "File does not exist: " + filename;
        }
        if (path_result == detail::path_type::directory) {
            return "File is actually a directory: " + filename;
        }
        return std::string();
    },
    "FILE"};

inline const Validator ExistingDirectory{
    [](std::string &filename) {
        auto path_result = detail::check_path(filename.c_str());
        if (path_result == detail::path_type::nonexistent) {
            return "Directory does not exist: " + filename;
        }
        if (path_result == detail::path_type::file) {
            return "Directory is actually a file: " + filename;
        }
        return std::string();
    },
    "DIR"};

inline const Validator ExistingPath{
    [](std::string &filename) {
        auto path_result = detail::check_path(filename.c_str());
        if (path_result == detail::path_type::nonexistent) {
            return "Path does not exist: " + filename;
        }
        return std::string();
    },
    "PATH(existing)"};

inline const Validator NonexistentPath{
    [](std::string &filename) {
        auto path_result = detail::check_path(filename.c_str());
        if (path_result != detail::path_type::nonexistent) {
            return "Path already exists: " + filename;
        }
        return std::string();
    },
    "PATH(non-existing)"};

inline const Validator EscapedString{
    [](std::string &str) {
        try {
            if (str.size() > 1 && (str.front() == '\"' || str.front() == '\'' || str.front() == '`') &&
                str.front() == str.back()) {
                detail::process_quoted_string(str);
            } else if (str.find_first_of('\\') != std::string::npos) {
                if (detail::is_binary_escaped_string(str)) {
                    str = detail::extract_binary_string(str);
                } else {
                    str = detail::remove_escaped_characters(str);
                }
            }
            return std::string{};
        } catch (const std::invalid_argument &ia) {
            return std::string(ia.what());
        }
    },
    std::string{}};

// --- FileOnDefaultPath ---

class FileOnDefaultPath : public Validator {
  public:
    explicit FileOnDefaultPath(std::string default_path, bool enableErrorReturn = true) : Validator("FILE") {
        func_ = [default_path, enableErrorReturn](std::string &filename) {
            auto path_result = detail::check_path(filename.c_str());
            if (path_result == detail::path_type::nonexistent) {
                std::string test_file_path = default_path;
                if (default_path.back() != '/' && default_path.back() != '\\') {
                    test_file_path += '/';
                }
                test_file_path.append(filename);
                path_result = detail::check_path(test_file_path.c_str());
                if (path_result == detail::path_type::file) {
                    filename = test_file_path;
                } else {
                    if (enableErrorReturn) {
                        return "File does not exist: " + filename;
                    }
                }
            }
            return std::string{};
        };
    }
};

// --- Range ---

class Range : public Validator {
  public:
    template <typename T>
    Range(T min_val, T max_val, const std::string &validator_name = std::string{}) : Validator(validator_name) {
        if (validator_name.empty()) {
            std::stringstream out;
            out << detail::type_name<T>() << " in [" << min_val << " - " << max_val << "]";
            description(out.str());
        }
        func_ = [min_val, max_val](std::string &input) {
            using CLI::detail::lexical_cast;
            T val;
            bool converted = lexical_cast(input, val);
            if ((!converted) || (val < min_val || val > max_val)) {
                std::stringstream out;
                out << "Value " << input << " not in range [";
                out << min_val << " - " << max_val << "]";
                return out.str();
            }
            return std::string{};
        };
    }

    template <typename T>
    explicit Range(T max_val, const std::string &validator_name = std::string{})
        : Range(static_cast<T>(0), max_val, validator_name) {}
};

inline const Range NonNegativeNumber((std::numeric_limits<double>::max)(), "NONNEGATIVE");

inline const Range PositiveNumber((std::numeric_limits<double>::min)(), (std::numeric_limits<double>::max)(),
                                  "POSITIVE");

// --- Overflow / multiply checks ---

namespace detail {

template <typename T>
inline std::enable_if_t<std::is_signed_v<T>, T> overflowCheck(const T &a, const T &b) {
    if ((a > 0) == (b > 0)) {
        return ((std::numeric_limits<T>::max)() / (std::abs)(a) < (std::abs)(b));
    }
    return ((std::numeric_limits<T>::min)() / (std::abs)(a) > -(std::abs)(b));
}

template <typename T>
inline std::enable_if_t<!std::is_signed_v<T>, T> overflowCheck(const T &a, const T &b) {
    return ((std::numeric_limits<T>::max)() / a < b);
}

template <typename T>
std::enable_if_t<std::is_integral_v<T>, bool> checked_multiply(T &a, T b) {
    if (a == 0 || b == 0 || a == 1 || b == 1) {
        a *= b;
        return true;
    }
    if (a == (std::numeric_limits<T>::min)() || b == (std::numeric_limits<T>::min)()) {
        return false;
    }
    if (overflowCheck(a, b)) {
        return false;
    }
    a *= b;
    return true;
}

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, bool> checked_multiply(T &a, T b) {
    T c = a * b;
    if (std::isinf(c) && !std::isinf(a) && !std::isinf(b)) {
        return false;
    }
    a = c;
    return true;
}

std::pair<std::string, std::string> split_program_name(std::string commandline) {
    std::pair<std::string, std::string> vals;
    trim(commandline);
    auto esp = commandline.find_first_of(' ', 1);
    while (check_path(commandline.substr(0, esp).c_str()) != path_type::file) {
        esp = commandline.find_first_of(' ', esp + 1);
        if (esp == std::string::npos) {
            if (commandline[0] == '"' || commandline[0] == '\'' || commandline[0] == '`') {
                bool embeddedQuote = false;
                auto keyChar = commandline[0];
                auto end = commandline.find_first_of(keyChar, 1);
                while ((end != std::string::npos) && (commandline[end - 1] == '\\')) {
                    end = commandline.find_first_of(keyChar, end + 1);
                    embeddedQuote = true;
                }
                if (end != std::string::npos) {
                    vals.first = commandline.substr(1, end - 1);
                    esp = end + 1;
                    if (embeddedQuote) {
                        vals.first =
                            find_and_replace(vals.first, std::string("\\") + keyChar, std::string(1, keyChar));
                    }
                } else {
                    esp = commandline.find_first_of(' ', 1);
                }
            } else {
                esp = commandline.find_first_of(' ', 1);
            }
            break;
        }
    }
    if (vals.first.empty()) {
        vals.first = commandline.substr(0, esp);
        rtrim(vals.first);
    }
    vals.second = (esp < commandline.length() - 1) ? commandline.substr(esp + 1) : std::string{};
    ltrim(vals.second);
    return vals;
}

}  // namespace detail

}  // namespace CLI
