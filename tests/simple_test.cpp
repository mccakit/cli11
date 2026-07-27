// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_helper.hpp"
import std;
import cli11;
import test_helper;

using input_t = std::vector<std::string>;

TEST_CASE("Basic: Empty", "[simple]")
{

    {
        cli::app_t app;
        input_t simpleput;
        app.parse(simpleput);
    }
    {
        cli::app_t app;
        input_t spare = {"spare"};
        CHECK_THROWS_AS(app.parse(spare), cli::extras_error_t);
    }
    {
        cli::app_t app;
        input_t simpleput;
        app.parse(simpleput);
    }
}
