// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#define CLI11_ENABLE_EXTRA_VALIDATORS 1

import std;
import cli11;

int main(int argc, char **argv)
{

    cli::app_t app("test for positional validation");

    int num1 {-1}, num2 {-1};
    app.add_option("num1", num1, "first number")->check(cli::number);
    app.add_option("num2", num2, "second number")->check(cli::number);
    std::string file1, file2;
    app.add_option("file1", file1, "first file")->required();
    app.add_option("file2", file2, "second file");
    app.validate_positionals();

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    if (num1 != -1)
        std::cout << "Num1 = " << num1 << '\n';

    if (num2 != -1)
        std::cout << "Num2 = " << num2 << '\n';

    std::cout << "File 1 = " << file1 << '\n';
    if (!file2.empty())
    {
        std::cout << "File 2 = " << file2 << '\n';
    }

    return 0;
}
