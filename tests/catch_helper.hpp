#pragma once

#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
using Catch::Approx;
using Catch::Generators::from_range;
using Catch::Matchers::Equals;
using Catch::Matchers::WithinRel;
inline auto Contains(const std::string &x)
{
    return Catch::Matchers::ContainsSubstring(x);
}
