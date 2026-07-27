// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char **argv)
{

    cli::app_t app("K3Pi goofit fitter");
    app.set_help_all_flag("--help-all", "Expand all help");
    app.add_flag("--random", "Some random flag");
    cli::app_t *start = app.add_subcommand("start", "A great subcommand");
    cli::app_t *stop = app.add_subcommand("stop", "Do you really want to stop?");
    app.require_subcommand(); // 1 or more

    std::string file;
    start->add_option("-f,--file", file, "File name");

    cli::option_t *s = stop->add_flag("-c,--count", "Counter");

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    std::cout << "Working on --file from start: " << file << '\n';
    std::cout << "Working on --count from stop: " << s->count() << ", direct count: " << stop->count("--count") << '\n';
    std::cout << "Count of --random flag: " << app.count("--random") << '\n';
    for (auto *subcom : app.get_subcommands())
        std::cout << "Subcommand: " << subcom->get_name() << '\n';

    return 0;
}
