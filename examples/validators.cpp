// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char **argv)
{

    CLI::App app("Validator checker");

    std::string file;
    app.add_option("-f,--file,file", file, "File name")->check(CLI::ExistingFile);

    int count {0};
    app.add_option("-v,--value", count, "Value in range")->check(CLI::Range(3, 6));
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }

    std::cout << "Try printing help or failing the validator" << '\n';

    return 0;
}
