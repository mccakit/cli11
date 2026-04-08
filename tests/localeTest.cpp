#include "catch_helper.hpp"

import std;
import cli11;
import test_helper;

class CustomThousandsSeparator : public std::numpunct<char>
{
    protected:
        char do_thousands_sep() const override
        {
            return '|';
        }
        std::string do_grouping() const override
        {
            return "\2";
        }
};

TEST_CASE_METHOD(TApp, "locale", "[separators]")
{
    std::locale customLocale(std::locale::classic(), new CustomThousandsSeparator);
    std::locale::global(customLocale);
    std::cout.imbue(std::locale());
    std::int64_t foo {0};
    std::uint64_t bar {0};
    float qux {0};
    app.add_option("FOO", foo, "Foo option")->default_val(1234567)->force_callback();
    app.add_option("BAR", bar, "Bar option")->default_val(2345678)->force_callback();
    app.add_option("QUX", qux, "QUX option")->default_val(3456.78)->force_callback();
    CHECK_NOTHROW(run());
    CHECK(foo == 1234567);
    CHECK(bar == 2345678);
    CHECK_THAT(qux, Catch::Matchers::WithinAbs(3456.78, 0.01));
}
