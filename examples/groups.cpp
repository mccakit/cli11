// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char **argv)
{
    cli::auto_timer_t give_me_a_name("This is a timer");

    cli::app_t app("K3Pi goofit fitter");

    std::string file;
    cli::option_t *opt = app.add_option("-f,--file,file", file, "File name")->required()->group("Important");

    int count {0};
    cli::option_t *copt = app.add_flag("-c,--count", count, "Counter")->required()->group("Important");

    double value {0.0}; // = 3.14;
    app.add_option("-d,--double", value, "Some Value")->group("Other");

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    std::cout << "Working on file: " << file << ", direct count: " << app.count("--file")
              << ", opt count: " << opt->count() << '\n';
    std::cout << "Working on count: " << count << ", direct count: " << app.count("--count")
              << ", opt count: " << copt->count() << '\n';
    std::cout << "Some value: " << value << '\n';

    return 0;
}
