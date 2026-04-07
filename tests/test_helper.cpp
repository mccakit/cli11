// test_cxxmodule/app_helper.cppm
module;

#include <cstdio>
#include <cstdlib>

export module test_helper;

import std;
import cli11;

export using input_t = std::vector<std::string>;

export class TApp {
  public:
    CLI::App app{"My Test Program"};
    input_t args{};
    virtual ~TApp() = default;
    void run() {
        input_t newargs = args;
        std::reverse(std::begin(newargs), std::end(newargs));
        app.parse(newargs);
    }
};

export int fileClear(const std::string &name) { return std::remove(name.c_str()); }

export class TempFile {
    std::string _name{};

  public:
    explicit TempFile(std::string name) : _name(std::move(name)) {
        if (!CLI::NonexistentPath(_name).empty())
            throw std::runtime_error(_name);
    }

    ~TempFile() {
        std::remove(_name.c_str());
    }

    operator const std::string &() const { return _name; }
    [[nodiscard]] const char *c_str() const { return _name.c_str(); }
};

export void put_env(std::string name, std::string value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

export void unset_env(std::string name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

export std::string from_u8string(const std::string &s) { return s; }
export std::string from_u8string(std::string &&s) { return std::move(s); }
export std::string from_u8string(const std::u8string &s) { return std::string(s.begin(), s.end()); }
