// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#define CLI11_ENABLE_EXTRA_VALIDATORS 1
import std;
import cli11;

// NOLINTNEXTLINE
enum class Level : int
{
    High,
    Medium,
    Low
};

int main(int argc, char **argv)
{
    cli::app_t app;

    Level level {Level::Low};
    // specify string->value mappings
    std::map<std::string, Level> map {{"high", Level::High}, {"medium", Level::Medium}, {"low", Level::Low}};
    // CheckedTransformer translates and checks whether the results are either in one of the strings or in one of the
    // translations already
    app.add_option("-l,--level", level, "Level settings")
        ->required()
        ->transform(cli::checked_transformer_t(map, cli::ignore_case));

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    // CLI11's built in enum streaming can be used outside CLI11 like this:
    using cli::enums::operator<<;
    std::cout << "Enum received: " << level << '\n';

    return 0;
}
