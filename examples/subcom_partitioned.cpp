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

    cli::app_ptr_t impOpt = std::make_shared<cli::app_t>("Important");
    std::string file;
    cli::option_t *opt = impOpt->add_option("-f,--file,file", file, "File name")->required();

    int count {0};
    cli::option_t *copt = impOpt->add_flag("-c,--count", count, "Counter")->required();

    cli::app_ptr_t otherOpt = std::make_shared<cli::app_t>("Other");
    double value {0.0}; // = 3.14;
    otherOpt->add_option("-d,--double", value, "Some Value");

    // add the subapps to the main one
    app.add_subcommand(impOpt);
    app.add_subcommand(otherOpt);

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    std::cout << "Working on file: " << file << ", direct count: " << impOpt->count("--file")
              << ", opt count: " << opt->count() << '\n';
    std::cout << "Working on count: " << count << ", direct count: " << impOpt->count("--count")
              << ", opt count: " << copt->count() << '\n';
    std::cout << "Some value: " << value << '\n';

    return 0;
}
