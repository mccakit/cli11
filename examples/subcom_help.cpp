// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char *argv[])
{
    CLI::App cli_global {"Demo app"};
    auto &cli_sub = *cli_global.add_subcommand("sub", "Some subcommand");
    std::string sub_arg;
    cli_sub.add_option("sub_arg", sub_arg, "Argument for subcommand")->required();
    try
    {
        cli_global.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return cli_global.exit(e);
    }
    if (cli_sub)
    {
        std::cout << "Got: " << sub_arg << '\n';
    }
    return 0;
}
