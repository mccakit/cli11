module;
#include <cerrno>
export module cli11:option;

import std;
import :string_tools;
import :error;
import :split;
import :validators;
import :type_tools;

export namespace CLI {

using results_t = std::vector<std::string>;
using callback_t = std::function<bool(const results_t &)>;

class Option;
class App;
class ConfigBase;

using Option_p = std::unique_ptr<Option>;
using Validator_p = std::shared_ptr<Validator>;

enum class MultiOptionPolicy : char {
    Throw,
    TakeLast,
    TakeFirst,
    Join,
    TakeAll,
    Sum,
    Reverse,
};

enum class CallbackPriority : std::uint8_t {
    FirstPreHelp = 0,
    First = 1,
    PreRequirementsCheckPreHelp = 2,
    PreRequirementsCheck = 3,
    NormalPreHelp = 4,
    Normal = 5,
    LastPreHelp = 6,
    Last = 7
};

template <typename CRTP>
class OptionBase {
    friend App;
    friend ConfigBase;

  protected:
    std::string group_ = std::string("OPTIONS");
    bool required_{false};
    bool ignore_case_{false};
    bool ignore_underscore_{false};
    bool configurable_{true};
    bool disable_flag_override_{false};
    char delimiter_{'\0'};
    bool always_capture_default_{false};
    MultiOptionPolicy multi_option_policy_{MultiOptionPolicy::Throw};
    CallbackPriority callback_priority_{CallbackPriority::Normal};

    template <typename T>
    void copy_to(T *other) const {
        other->group(group_);
        other->required(required_);
        other->ignore_case(ignore_case_);
        other->ignore_underscore(ignore_underscore_);
        other->configurable(configurable_);
        other->disable_flag_override(disable_flag_override_);
        other->delimiter(delimiter_);
        other->always_capture_default(always_capture_default_);
        other->multi_option_policy(multi_option_policy_);
        other->callback_priority(callback_priority_);
    }

  public:
    CRTP *group(const std::string &name) {
        if (!detail::valid_alias_name_string(name)) {
            throw IncorrectConstruction("Group names may not contain newlines or null characters");
        }
        group_ = name;
        return static_cast<CRTP *>(this);
    }

    CRTP *required(bool value = true) {
        required_ = value;
        return static_cast<CRTP *>(this);
    }

    CRTP *mandatory(bool value = true) { return required(value); }

    CRTP *always_capture_default(bool value = true) {
        always_capture_default_ = value;
        return static_cast<CRTP *>(this);
    }

    [[nodiscard]] const std::string &get_group() const { return group_; }
    [[nodiscard]] bool get_required() const { return required_; }
    [[nodiscard]] bool get_ignore_case() const { return ignore_case_; }
    [[nodiscard]] bool get_ignore_underscore() const { return ignore_underscore_; }
    [[nodiscard]] bool get_configurable() const { return configurable_; }
    [[nodiscard]] bool get_disable_flag_override() const { return disable_flag_override_; }
    [[nodiscard]] char get_delimiter() const { return delimiter_; }
    [[nodiscard]] bool get_always_capture_default() const { return always_capture_default_; }
    [[nodiscard]] MultiOptionPolicy get_multi_option_policy() const { return multi_option_policy_; }
    [[nodiscard]] CallbackPriority get_callback_priority() const { return callback_priority_; }

    CRTP *take_last() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeLast);
        return self;
    }

    CRTP *take_first() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeFirst);
        return self;
    }

    CRTP *take_all() {
        auto self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeAll);
        return self;
    }

    CRTP *join() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::Join);
        return self;
    }

    CRTP *join(char delim) {
        auto self = static_cast<CRTP *>(this);
        self->delimiter_ = delim;
        self->multi_option_policy(MultiOptionPolicy::Join);
        return self;
    }

    CRTP *configurable(bool value = true) {
        configurable_ = value;
        return static_cast<CRTP *>(this);
    }

    CRTP *delimiter(char value = '\0') {
        delimiter_ = value;
        return static_cast<CRTP *>(this);
    }
};

class OptionDefaults : public OptionBase<OptionDefaults> {
  public:
    OptionDefaults() = default;

    OptionDefaults *callback_priority(CallbackPriority value = CallbackPriority::Normal) {
        callback_priority_ = value;
        return this;
    }

    OptionDefaults *multi_option_policy(MultiOptionPolicy value = MultiOptionPolicy::Throw) {
        multi_option_policy_ = value;
        return this;
    }

    OptionDefaults *ignore_case(bool value = true) {
        ignore_case_ = value;
        return this;
    }

    OptionDefaults *ignore_underscore(bool value = true) {
        ignore_underscore_ = value;
        return this;
    }

    OptionDefaults *disable_flag_override(bool value = true) {
        disable_flag_override_ = value;
        return this;
    }

    OptionDefaults *delimiter(char value = '\0') {
        delimiter_ = value;
        return this;
    }
};

class Option : public OptionBase<Option> {
    friend App;
    friend ConfigBase;

  protected:
    std::vector<std::string> snames_{};
    std::vector<std::string> lnames_{};
    std::vector<std::pair<std::string, std::string>> default_flag_values_{};
    std::vector<std::string> fnames_{};
    std::string pname_{};
    std::string envname_{};

    std::string description_{};
    std::string default_str_{};
    std::string option_text_{};
    std::function<std::string()> type_name_{[]() { return std::string(); }};
    std::function<std::string()> default_function_{};

    int type_size_max_{1};
    int type_size_min_{1};
    int expected_min_{1};
    int expected_max_{1};

    std::vector<Validator_p> validators_{};
    std::set<Option *> needs_{};
    std::set<Option *> excludes_{};

    App *parent_{nullptr};
    callback_t callback_{};

    results_t results_{};
    mutable results_t proc_results_{};

    enum class option_state : char {
        parsing = 0,
        validated = 2,
        reduced = 4,
        callback_run = 6,
    };

    option_state current_option_state_{option_state::parsing};
    bool allow_extra_args_{false};
    bool flag_like_{false};
    bool run_callback_for_default_{false};
    bool inject_separator_{false};
    bool trigger_on_result_{false};
    bool force_callback_{false};

    Option(std::string option_name, std::string option_description, callback_t callback, App *parent,
           bool allow_non_standard = false)
        : description_(std::move(option_description)), parent_(parent), callback_(std::move(callback)) {
        std::tie(snames_, lnames_, pname_) = detail::get_names(detail::split_names(option_name), allow_non_standard);
    }

  public:
    Option(const Option &) = delete;
    Option &operator=(const Option &) = delete;

    [[nodiscard]] std::size_t count() const { return results_.size(); }
    [[nodiscard]] bool empty() const { return results_.empty(); }
    explicit operator bool() const { return !empty() || force_callback_; }

    void clear() {
        results_.clear();
        current_option_state_ = option_state::parsing;
    }

    // --- Setting options ---

    Option *expected(int value) {
        if (value < 0) {
            expected_min_ = -value;
            if (expected_max_ < expected_min_) {
                expected_max_ = expected_min_;
            }
            allow_extra_args_ = true;
            flag_like_ = false;
        } else if (value == detail::expected_max_vector_size) {
            expected_min_ = 1;
            expected_max_ = detail::expected_max_vector_size;
            allow_extra_args_ = true;
            flag_like_ = false;
        } else {
            expected_min_ = value;
            expected_max_ = value;
            flag_like_ = (expected_min_ == 0);
        }
        return this;
    }

    Option *expected(int value_min, int value_max) {
        if (value_min < 0) {
            value_min = -value_min;
        }
        if (value_max < 0) {
            value_max = detail::expected_max_vector_size;
        }
        if (value_max < value_min) {
            expected_min_ = value_max;
            expected_max_ = value_min;
        } else {
            expected_max_ = value_max;
            expected_min_ = value_min;
        }
        return this;
    }

    Option *allow_extra_args(bool value = true) {
        allow_extra_args_ = value;
        return this;
    }
    [[nodiscard]] bool get_allow_extra_args() const { return allow_extra_args_; }

    Option *trigger_on_parse(bool value = true) {
        trigger_on_result_ = value;
        return this;
    }
    [[nodiscard]] bool get_trigger_on_parse() const { return trigger_on_result_; }

    Option *force_callback(bool value = true) {
        force_callback_ = value;
        return this;
    }
    [[nodiscard]] bool get_force_callback() const { return force_callback_; }

    Option *run_callback_for_default(bool value = true) {
        run_callback_for_default_ = value;
        return this;
    }
    [[nodiscard]] bool get_run_callback_for_default() const { return run_callback_for_default_; }

    Option *callback_priority(CallbackPriority value = CallbackPriority::Normal) {
        callback_priority_ = value;
        return this;
    }

    Option *check(Validator_p validator) {
        validator->non_modifying();
        validators_.push_back(std::move(validator));
        return this;
    }

    Option *check(Validator validator, const std::string &validator_name = "") {
        validator.non_modifying();
        auto vp = std::make_shared<Validator>(std::move(validator));
        if (!validator_name.empty()) {
            vp->name(validator_name);
        }
        validators_.push_back(std::move(vp));
        return this;
    }

    Option *check(std::function<std::string(const std::string &)> validator_func, std::string validator_description = "",
                  std::string validator_name = "") {
        auto vp = std::make_shared<Validator>(std::move(validator_func), std::move(validator_description),
                                              std::move(validator_name));
        vp->non_modifying();
        validators_.push_back(std::move(vp));
        return this;
    }

    Option *transform(Validator_p validator) {
        validators_.insert(validators_.begin(), std::move(validator));
        return this;
    }

    Option *transform(Validator validator, const std::string &transform_name = "") {
        auto vp = std::make_shared<Validator>(std::move(validator));
        if (!transform_name.empty()) {
            vp->name(transform_name);
        }
        validators_.insert(validators_.begin(), std::move(vp));
        return this;
    }

    Option *transform(const std::function<std::string(std::string)> &transform_func,
                      std::string transform_description = "", std::string transform_name = "") {
        auto vp = std::make_shared<Validator>(
            [transform_func](std::string &val) {
                val = transform_func(val);
                return std::string{};
            },
            std::move(transform_description), std::move(transform_name));
        validators_.insert(validators_.begin(), std::move(vp));
        return this;
    }

    Option *each(const std::function<void(std::string)> &func) {
        auto vp = std::make_shared<Validator>(
            [func](std::string &inout) {
                func(inout);
                return std::string{};
            },
            std::string{});
        validators_.push_back(std::move(vp));
        return this;
    }

    Validator *get_validator(const std::string &validator_name = "") {
        for (auto &validator : validators_) {
            if (validator_name == validator->get_name()) {
                return validator.get();
            }
        }
        if ((validator_name.empty()) && (!validators_.empty())) {
            return validators_.front().get();
        }
        throw OptionNotFound(std::string{"Validator "} + validator_name + " Not Found");
    }

    Validator *get_validator(int index) {
        if (index >= 0 && index < static_cast<int>(validators_.size())) {
            return validators_[static_cast<decltype(validators_)::size_type>(index)].get();
        }
        throw OptionNotFound("Validator index is not valid");
    }

    Option *needs(Option *opt) {
        if (opt != this) {
            needs_.insert(opt);
        }
        return this;
    }

    template <typename T = App>
    Option *needs(std::string opt_name) {
        auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
        if (opt == nullptr) {
            throw IncorrectConstruction::MissingOption(opt_name);
        }
        return needs(opt);
    }

    template <typename A, typename B, typename... ARG>
    Option *needs(A opt, B opt1, ARG... args) {
        needs(opt);
        return needs(opt1, args...);
    }

    bool remove_needs(Option *opt) {
        auto iterator = std::find(std::begin(needs_), std::end(needs_), opt);
        if (iterator == std::end(needs_)) {
            return false;
        }
        needs_.erase(iterator);
        return true;
    }

    Option *excludes(Option *opt) {
        if (opt == this) {
            throw(IncorrectConstruction("and option cannot exclude itself"));
        }
        excludes_.insert(opt);
        opt->excludes_.insert(this);
        return this;
    }

    template <typename T = App>
    Option *excludes(std::string opt_name) {
        auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
        if (opt == nullptr) {
            throw IncorrectConstruction::MissingOption(opt_name);
        }
        return excludes(opt);
    }

    template <typename A, typename B, typename... ARG>
    Option *excludes(A opt, B opt1, ARG... args) {
        excludes(opt);
        return excludes(opt1, args...);
    }

    bool remove_excludes(Option *opt) {
        auto iterator = std::find(std::begin(excludes_), std::end(excludes_), opt);
        if (iterator == std::end(excludes_)) {
            return false;
        }
        excludes_.erase(iterator);
        return true;
    }

    Option *envname(std::string name) {
        envname_ = std::move(name);
        return this;
    }

    template <typename T = App>
    Option *ignore_case(bool value = true) {
        if (!ignore_case_ && value) {
            ignore_case_ = value;
            auto *parent = static_cast<T *>(parent_);
            for (const Option_p &opt : parent->options_) {
                if (opt.get() == this) {
                    continue;
                }
                const auto &omatch = opt->matching_name(*this);
                if (!omatch.empty()) {
                    ignore_case_ = false;
                    throw OptionAlreadyAdded("adding ignore case caused a name conflict with " + omatch);
                }
            }
        } else {
            ignore_case_ = value;
        }
        return this;
    }

    template <typename T = App>
    Option *ignore_underscore(bool value = true) {
        if (!ignore_underscore_ && value) {
            ignore_underscore_ = value;
            auto *parent = static_cast<T *>(parent_);
            for (const Option_p &opt : parent->options_) {
                if (opt.get() == this) {
                    continue;
                }
                const auto &omatch = opt->matching_name(*this);
                if (!omatch.empty()) {
                    ignore_underscore_ = false;
                    throw OptionAlreadyAdded("adding ignore underscore caused a name conflict with " + omatch);
                }
            }
        } else {
            ignore_underscore_ = value;
        }
        return this;
    }

    Option *multi_option_policy(MultiOptionPolicy value = MultiOptionPolicy::Throw) {
        if (value != multi_option_policy_) {
            if (multi_option_policy_ == MultiOptionPolicy::Throw && expected_max_ == detail::expected_max_vector_size &&
                expected_min_ > 1) {
                expected_max_ = expected_min_;
            }
            multi_option_policy_ = value;
            current_option_state_ = option_state::parsing;
        }
        return this;
    }

    Option *disable_flag_override(bool value = true) {
        disable_flag_override_ = value;
        return this;
    }

    // --- Accessors ---

    [[nodiscard]] int get_type_size() const { return type_size_min_; }
    [[nodiscard]] int get_type_size_min() const { return type_size_min_; }
    [[nodiscard]] int get_type_size_max() const { return type_size_max_; }
    [[nodiscard]] bool get_inject_separator() const { return inject_separator_; }
    [[nodiscard]] std::string get_envname() const { return envname_; }
    [[nodiscard]] std::set<Option *> get_needs() const { return needs_; }
    [[nodiscard]] std::set<Option *> get_excludes() const { return excludes_; }
    [[nodiscard]] std::string get_default_str() const { return default_str_; }
    [[nodiscard]] callback_t get_callback() const { return callback_; }
    [[nodiscard]] const std::vector<std::string> &get_lnames() const { return lnames_; }
    [[nodiscard]] const std::vector<std::string> &get_snames() const { return snames_; }
    [[nodiscard]] const std::vector<std::string> &get_fnames() const { return fnames_; }

    [[nodiscard]] const std::string &get_single_name() const {
        if (!lnames_.empty()) {
            return lnames_[0];
        }
        if (!snames_.empty()) {
            return snames_[0];
        }
        if (!pname_.empty()) {
            return pname_;
        }
        return envname_;
    }

    [[nodiscard]] int get_expected() const { return expected_min_; }
    [[nodiscard]] int get_expected_min() const { return expected_min_; }
    [[nodiscard]] int get_expected_max() const { return expected_max_; }
    [[nodiscard]] int get_items_expected_min() const { return type_size_min_ * expected_min_; }

    [[nodiscard]] int get_items_expected_max() const {
        int t = type_size_max_;
        return detail::checked_multiply(t, expected_max_) ? t : detail::expected_max_vector_size;
    }

    [[nodiscard]] int get_items_expected() const { return get_items_expected_min(); }
    [[nodiscard]] bool get_positional() const { return !pname_.empty(); }
    [[nodiscard]] bool nonpositional() const { return (!lnames_.empty() || !snames_.empty()); }
    [[nodiscard]] bool has_description() const { return !description_.empty(); }
    [[nodiscard]] const std::string &get_description() const { return description_; }

    Option *description(std::string option_description) {
        description_ = std::move(option_description);
        return this;
    }

    Option *option_text(std::string text) {
        option_text_ = std::move(text);
        return this;
    }

    [[nodiscard]] const std::string &get_option_text() const { return option_text_; }

    // --- Help tools ---

    [[nodiscard]] std::string get_name(bool positional = false, bool all_options = false,
                                       bool disable_default_flag_values = false) const {
        if (get_group().empty())
            return {};

        if (all_options) {
            std::vector<std::string> name_list;
            if ((positional && (!pname_.empty())) || (snames_.empty() && lnames_.empty())) {
                name_list.push_back(pname_);
            }
            if ((get_items_expected() == 0) && (!fnames_.empty())) {
                for (const std::string &sname : snames_) {
                    name_list.push_back("-" + sname);
                    if (!disable_default_flag_values && check_fname(sname)) {
                        name_list.back() += "{" + get_flag_value(sname, "") + "}";
                    }
                }
                for (const std::string &lname : lnames_) {
                    name_list.push_back("--" + lname);
                    if (!disable_default_flag_values && check_fname(lname)) {
                        name_list.back() += "{" + get_flag_value(lname, "") + "}";
                    }
                }
            } else {
                for (const std::string &sname : snames_)
                    name_list.push_back("-" + sname);
                for (const std::string &lname : lnames_)
                    name_list.push_back("--" + lname);
            }
            return detail::join(name_list);
        }

        if (positional)
            return pname_;
        if (!lnames_.empty())
            return std::string(2, '-') + lnames_[0];
        if (!snames_.empty())
            return std::string(1, '-') + snames_[0];
        return pname_;
    }

    // --- Parser tools ---

    void run_callback() {
        bool used_default_str = false;
        if (force_callback_ && results_.empty()) {
            used_default_str = true;
            add_result(default_str_);
        }
        if (current_option_state_ == option_state::parsing) {
            _validate_results(results_);
            current_option_state_ = option_state::validated;
        }
        if (current_option_state_ < option_state::reduced) {
            _reduce_results(proc_results_, results_);
        }
        current_option_state_ = option_state::callback_run;
        if (callback_) {
            const results_t &send_results = proc_results_.empty() ? results_ : proc_results_;
            if (send_results.empty()) {
                return;
            }
            bool local_result = callback_(send_results);
            if (used_default_str) {
                results_.clear();
                proc_results_.clear();
            }
            if (!local_result)
                throw ConversionError(get_name(), results_);
        }
    }

    [[nodiscard]] const std::string &matching_name(const Option &other) const {
        static const std::string estring;
        bool bothConfigurable = configurable_ && other.configurable_;
        for (const std::string &sname : snames_) {
            if (other.check_sname(sname))
                return sname;
            if (bothConfigurable && other.check_lname(sname))
                return sname;
        }
        for (const std::string &lname : lnames_) {
            if (other.check_lname(lname))
                return lname;
            if (lname.size() == 1 && bothConfigurable) {
                if (other.check_sname(lname)) {
                    return lname;
                }
            }
        }
        if (bothConfigurable && snames_.empty() && lnames_.empty() && !pname_.empty()) {
            if (other.check_sname(pname_) || other.check_lname(pname_) || pname_ == other.pname_)
                return pname_;
        }
        if (bothConfigurable && other.snames_.empty() && other.fnames_.empty() && !other.pname_.empty()) {
            if (check_sname(other.pname_) || check_lname(other.pname_) || (pname_ == other.pname_))
                return other.pname_;
        }
        if (ignore_case_ || ignore_underscore_) {
            for (const std::string &sname : other.snames_)
                if (check_sname(sname))
                    return sname;
            for (const std::string &lname : other.lnames_)
                if (check_lname(lname))
                    return lname;
        }
        return estring;
    }

    bool operator==(const Option &other) const { return !matching_name(other).empty(); }

    [[nodiscard]] bool check_name(const std::string &name) const {
        if (name.length() > 2 && name[0] == '-' && name[1] == '-')
            return check_lname(name.substr(2));
        if (name.length() > 1 && name.front() == '-')
            return check_sname(name.substr(1));
        if (!pname_.empty()) {
            std::string local_pname = pname_;
            std::string local_name = name;
            if (ignore_underscore_) {
                local_pname = detail::remove_underscore(local_pname);
                local_name = detail::remove_underscore(local_name);
            }
            if (ignore_case_) {
                local_pname = detail::to_lower(local_pname);
                local_name = detail::to_lower(local_name);
            }
            if (local_name == local_pname) {
                return true;
            }
        }
        if (!envname_.empty()) {
            return (name == envname_);
        }
        return false;
    }

    [[nodiscard]] bool check_sname(std::string name) const {
        return (detail::find_member(std::move(name), snames_, ignore_case_) >= 0);
    }

    [[nodiscard]] bool check_lname(std::string name) const {
        return (detail::find_member(std::move(name), lnames_, ignore_case_, ignore_underscore_) >= 0);
    }

    [[nodiscard]] bool check_fname(std::string name) const {
        if (fnames_.empty()) {
            return false;
        }
        return (detail::find_member(std::move(name), fnames_, ignore_case_, ignore_underscore_) >= 0);
    }

    [[nodiscard]] std::string get_flag_value(const std::string &name, std::string input_value) const {
        static const std::string trueString{"true"};
        static const std::string falseString{"false"};
        static const std::string emptyString{"{}"};
        if (disable_flag_override_) {
            if (!((input_value.empty()) || (input_value == emptyString))) {
                auto default_ind = detail::find_member(name, fnames_, ignore_case_, ignore_underscore_);
                if (default_ind >= 0) {
                    if (default_flag_values_[static_cast<std::size_t>(default_ind)].second != input_value) {
                        if (input_value == default_str_ && force_callback_) {
                            return input_value;
                        }
                        throw(ArgumentMismatch::FlagOverride(name));
                    }
                } else {
                    if (input_value != trueString) {
                        throw(ArgumentMismatch::FlagOverride(name));
                    }
                }
            }
        }
        auto ind = detail::find_member(name, fnames_, ignore_case_, ignore_underscore_);
        if ((input_value.empty()) || (input_value == emptyString)) {
            if (flag_like_) {
                return (ind < 0) ? trueString : default_flag_values_[static_cast<std::size_t>(ind)].second;
            }
            return (ind < 0) ? default_str_ : default_flag_values_[static_cast<std::size_t>(ind)].second;
        }
        if (ind < 0) {
            return input_value;
        }
        if (default_flag_values_[static_cast<std::size_t>(ind)].second == falseString) {
            errno = 0;
            auto val = detail::to_flag_value(input_value);
            if (errno != 0) {
                errno = 0;
                return input_value;
            }
            return (val == 1) ? falseString : (val == (-1) ? trueString : std::to_string(-val));
        }
        return input_value;
    }

    Option *add_result(std::string s) {
        _add_result(std::move(s), results_);
        current_option_state_ = option_state::parsing;
        return this;
    }

    Option *add_result(std::string s, int &results_added) {
        results_added = _add_result(std::move(s), results_);
        current_option_state_ = option_state::parsing;
        return this;
    }

    Option *add_result(std::vector<std::string> s) {
        current_option_state_ = option_state::parsing;
        for (auto &str : s) {
            _add_result(std::move(str), results_);
        }
        return this;
    }

    [[nodiscard]] const results_t &results() const { return results_; }

    [[nodiscard]] results_t reduced_results() const {
        results_t res = proc_results_.empty() ? results_ : proc_results_;
        if (current_option_state_ < option_state::reduced) {
            if (current_option_state_ == option_state::parsing) {
                res = results_;
                _validate_results(res);
            }
            if (!res.empty()) {
                results_t extra;
                _reduce_results(extra, res);
                if (!extra.empty()) {
                    res = std::move(extra);
                }
            }
        }
        return res;
    }

    template <typename T>
    void results(T &output) const {
        bool retval = false;
        if (current_option_state_ >= option_state::reduced || (results_.size() == 1 && validators_.empty())) {
            const results_t &res = (proc_results_.empty()) ? results_ : proc_results_;
            if (!res.empty()) {
                retval = detail::lexical_conversion<T, T>(res, output);
            } else {
                results_t res2;
                res2.emplace_back();
                proc_results_ = std::move(res2);
                retval = detail::lexical_conversion<T, T>(proc_results_, output);
            }
        } else {
            results_t res;
            if (results_.empty()) {
                if (!default_str_.empty()) {
                    _add_result(std::string(default_str_), res);
                    _validate_results(res);
                    results_t extra;
                    _reduce_results(extra, res);
                    if (!extra.empty()) {
                        res = std::move(extra);
                    }
                } else {
                    res.emplace_back();
                }
            } else {
                res = reduced_results();
            }
            proc_results_ = std::move(res);
            retval = detail::lexical_conversion<T, T>(proc_results_, output);
        }
        if (!retval) {
            throw ConversionError(get_name(), results_);
        }
    }

    template <typename T>
    [[nodiscard]] T as() const {
        T output;
        results(output);
        return output;
    }

    [[nodiscard]] bool get_callback_run() const { return (current_option_state_ == option_state::callback_run); }

    // --- Custom options ---

    Option *type_name_fn(std::function<std::string()> typefun) {
        type_name_ = std::move(typefun);
        return this;
    }

    Option *type_name(std::string typeval) {
        type_name_fn([typeval]() { return typeval; });
        return this;
    }

    Option *type_size(int option_type_size) {
        if (option_type_size < 0) {
            type_size_max_ = -option_type_size;
            type_size_min_ = -option_type_size;
            expected_max_ = detail::expected_max_vector_size;
        } else {
            type_size_max_ = option_type_size;
            if (type_size_max_ < detail::expected_max_vector_size) {
                type_size_min_ = option_type_size;
            } else {
                inject_separator_ = true;
            }
            if (type_size_max_ == 0)
                required_ = false;
        }
        return this;
    }

    Option *type_size(int option_type_size_min, int option_type_size_max) {
        if (option_type_size_min < 0 || option_type_size_max < 0) {
            expected_max_ = detail::expected_max_vector_size;
            option_type_size_min = (std::abs)(option_type_size_min);
            option_type_size_max = (std::abs)(option_type_size_max);
        }
        if (option_type_size_min > option_type_size_max) {
            type_size_max_ = option_type_size_min;
            type_size_min_ = option_type_size_max;
        } else {
            type_size_min_ = option_type_size_min;
            type_size_max_ = option_type_size_max;
        }
        if (type_size_max_ == 0) {
            required_ = false;
        }
        if (type_size_max_ >= detail::expected_max_vector_size) {
            inject_separator_ = true;
        }
        return this;
    }

    void inject_separator(bool value = true) { inject_separator_ = value; }

    Option *default_function(const std::function<std::string()> &func) {
        default_function_ = func;
        return this;
    }

    Option *capture_default_str() {
        if (default_function_) {
            default_str_ = default_function_();
        }
        return this;
    }

    Option *default_str(std::string val) {
        default_str_ = std::move(val);
        return this;
    }

    template <typename X>
    Option *default_val(const X &val) {
        std::string val_str = detail::value_string(val);
        auto old_option_state = current_option_state_;
        results_t old_results{std::move(results_)};
        results_.clear();
        try {
            add_result(val_str);
            if (run_callback_for_default_ && !trigger_on_result_) {
                run_callback();
                current_option_state_ = option_state::parsing;
            } else {
                _validate_results(results_);
                current_option_state_ = old_option_state;
            }
        } catch (const ConversionError &err) {
            results_ = std::move(old_results);
            current_option_state_ = old_option_state;
            throw ConversionError(get_name(),
                                  std::string("given default value(\"") + val_str + "\") produces an error : " + err.what());
        } catch (const CLI::Error &) {
            results_ = std::move(old_results);
            current_option_state_ = old_option_state;
            throw;
        }
        results_ = std::move(old_results);
        default_str_ = std::move(val_str);
        return this;
    }

    [[nodiscard]] std::string get_type_name() const {
        std::string full_type_name = type_name_();
        if (!validators_.empty()) {
            for (const auto &validator : validators_) {
                std::string vtype = validator->get_description();
                if (!vtype.empty()) {
                    full_type_name += ":" + vtype;
                }
            }
        }
        return full_type_name;
    }

  private:
    void _validate_results(results_t &res) const {
        if (!validators_.empty()) {
            if (type_size_max_ > 1) {
                int index = 0;
                if (get_items_expected_max() < static_cast<int>(res.size()) &&
                    (multi_option_policy_ == MultiOptionPolicy::TakeLast ||
                     multi_option_policy_ == MultiOptionPolicy::Reverse)) {
                    index = get_items_expected_max() - static_cast<int>(res.size());
                }
                for (std::string &result : res) {
                    if (detail::is_separator(result) && type_size_max_ != type_size_min_ && index >= 0) {
                        index = 0;
                        continue;
                    }
                    auto err_msg = _validate(result, (index >= 0) ? (index % type_size_max_) : index);
                    if (!err_msg.empty())
                        throw ValidationError(get_name(), err_msg);
                    ++index;
                }
            } else {
                int index = 0;
                if (expected_max_ < static_cast<int>(res.size()) &&
                    (multi_option_policy_ == MultiOptionPolicy::TakeLast ||
                     multi_option_policy_ == MultiOptionPolicy::Reverse)) {
                    index = expected_max_ - static_cast<int>(res.size());
                }
                for (std::string &result : res) {
                    auto err_msg = _validate(result, index);
                    ++index;
                    if (!err_msg.empty())
                        throw ValidationError(get_name(), err_msg);
                }
            }
        }
    }

    void _reduce_results(results_t &out, const results_t &original) const {
        out.clear();
        switch (multi_option_policy_) {
        case MultiOptionPolicy::TakeAll:
            break;
        case MultiOptionPolicy::TakeLast: {
            std::size_t trim_size = std::min<std::size_t>(
                static_cast<std::size_t>(std::max<int>(get_items_expected_max(), 1)), original.size());
            if (original.size() != trim_size) {
                out.assign(original.end() - static_cast<results_t::difference_type>(trim_size), original.end());
            }
        } break;
        case MultiOptionPolicy::Reverse: {
            std::size_t trim_size = std::min<std::size_t>(
                static_cast<std::size_t>(std::max<int>(get_items_expected_max(), 1)), original.size());
            if (original.size() != trim_size || trim_size > 1) {
                out.assign(original.end() - static_cast<results_t::difference_type>(trim_size), original.end());
            }
            std::reverse(out.begin(), out.end());
        } break;
        case MultiOptionPolicy::TakeFirst: {
            std::size_t trim_size = std::min<std::size_t>(
                static_cast<std::size_t>(std::max<int>(get_items_expected_max(), 1)), original.size());
            if (original.size() != trim_size) {
                out.assign(original.begin(), original.begin() + static_cast<results_t::difference_type>(trim_size));
            }
        } break;
        case MultiOptionPolicy::Join:
            if (results_.size() > 1) {
                out.push_back(detail::join(original, std::string(1, (delimiter_ == '\0') ? '\n' : delimiter_)));
            }
            break;
        case MultiOptionPolicy::Sum:
            out.push_back(detail::sum_string_vector(original));
            break;
        case MultiOptionPolicy::Throw:
        default: {
            auto num_min = static_cast<std::size_t>(get_items_expected_min());
            auto num_max = static_cast<std::size_t>(get_items_expected_max());
            if (num_min == 0) {
                num_min = 1;
            }
            if (num_max == 0) {
                num_max = 1;
            }
            if (original.size() < num_min) {
                throw ArgumentMismatch::AtLeast(get_name(), static_cast<int>(num_min), original.size());
            }
            if (original.size() > num_max) {
                if (original.size() == 2 && num_max == 1 && original[1] == "%%" && original[0] == "{}") {
                    out = original;
                } else {
                    throw ArgumentMismatch::AtMost(get_name(), static_cast<int>(num_max), original.size());
                }
            }
            break;
        }
        }
        if (out.empty()) {
            if (original.size() == 1 && original[0] == "{}" && get_items_expected_min() > 0) {
                out.emplace_back("{}");
                out.emplace_back("%%");
            }
        } else if (out.size() == 1 && out[0] == "{}" && get_items_expected_min() > 0) {
            out.emplace_back("%%");
        }
    }

    std::string _validate(std::string &result, int index) const {
        std::string err_msg;
        if (result.empty() && expected_min_ == 0) {
            return err_msg;
        }
        for (const auto &vali : validators_) {
            auto v = vali->get_application_index();
            if (v == -1 || v == index) {
                try {
                    err_msg = (*vali)(result);
                } catch (const ValidationError &err) {
                    err_msg = err.what();
                }
                if (!err_msg.empty())
                    break;
            }
        }
        return err_msg;
    }

    int _add_result(std::string &&result, std::vector<std::string> &res) const {
        int result_count = 0;
        if (result.size() >= 4 && result[0] == '[' && result[1] == '[' && result.back() == ']' &&
            (*(result.end() - 2) == ']')) {
            std::string nstrs{'['};
            bool duplicated{true};
            for (std::size_t ii = 2; ii < result.size() - 2; ii += 2) {
                if (result[ii] == result[ii + 1]) {
                    nstrs.push_back(result[ii]);
                } else {
                    duplicated = false;
                    break;
                }
            }
            if (duplicated) {
                nstrs.push_back(']');
                res.push_back(std::move(nstrs));
                ++result_count;
                return result_count;
            }
        }
        if ((allow_extra_args_ || get_expected_max() > 1 || get_type_size() > 1) && !result.empty() &&
            result.front() == '[' && result.back() == ']') {
            result.pop_back();
            result.erase(result.begin());
            bool skipSection{false};
            for (auto &var : detail::split_up(result, ',')) {
                if (!var.empty()) {
                    result_count += _add_result(std::move(var), res);
                }
            }
            if (!skipSection) {
                return result_count;
            }
        }
        if (delimiter_ == '\0') {
            res.push_back(std::move(result));
            ++result_count;
        } else {
            if ((result.find_first_of(delimiter_) != std::string::npos)) {
                for (const auto &var : detail::split(result, delimiter_)) {
                    if (!var.empty()) {
                        res.push_back(var);
                        ++result_count;
                    }
                }
            } else {
                res.push_back(std::move(result));
                ++result_count;
            }
        }
        return result_count;
    }
};

}  // namespace CLI
