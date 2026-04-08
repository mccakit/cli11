// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char **argv)
{

    CLI::App app("K3Pi goofit fitter");
    // add version output
    app.set_version_flag("--version", std::string(CLI::version));
    std::string file;
    CLI::Option *opt = app.add_option("-f,--file,file", file, "File name");

    int count {0};
    CLI::Option *copt = app.add_option("-c,--count", count, "Counter");

    int v {0};
    CLI::Option *flag = app.add_flag("--flag", v, "Some flag that can be passed multiple times");

    double value {0.0}; // = 3.14;
    app.add_option("-d,--double", value, "Some Value");

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }

    std::cout << "Working on file: " << file << ", direct count: " << app.count("--file")
              << ", opt count: " << opt->count() << '\n';
    std::cout << "Working on count: " << count << ", direct count: " << app.count("--count")
              << ", opt count: " << copt->count() << '\n';
    std::cout << "Received flag: " << v << " (" << flag->count() << ") times\n";
    std::cout << "Some value: " << value << '\n';

    return 0;
}
