// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
#include <Windows.h>
#include "catch_helper.hpp"
import std;
import cli11;
import test_helper;
// This test verifies that CLI11 still works if
// Windows.h is included. #145

TEST_CASE_METHOD(TApp, "WindowsTestSimple", "[windows]")
{
    app.add_flag("-c,--count");
    args = {"-c"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
}
