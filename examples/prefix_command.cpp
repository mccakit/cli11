// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

import std;
import cli11;

int main(int argc, char **argv)
{

    cli::app_t app("Prefix command app");
    app.prefix_command(cli::prefix_command_mode_t::on);

    std::vector<int> vals;
    app.add_option("--vals,-v", vals)->expected(-1);

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    std::vector<std::string> more_comms = app.remaining();

    std::cout << "Prefix";
    for (int v : vals)
        std::cout << ": " << v << " ";

    std::cout << '\n' << "Remaining commands: ";

    for (const auto &com : more_comms)
        std::cout << com << " ";
    std::cout << '\n';

    return 0;
}
