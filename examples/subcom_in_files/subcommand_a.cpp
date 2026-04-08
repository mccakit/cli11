export module subcommand_a;

import std;
import cli11;

export struct SubcommandAOptions {
    std::string file;
    bool with_foo;
};

export void setup_subcommand_a(CLI::App &app) {
    auto opt = std::make_shared<SubcommandAOptions>();
    auto *sub = app.add_subcommand("subcommand_a", "performs subcommand a");
    sub->add_option("-f,--file", opt->file, "File name")->required();
    sub->add_flag("--with-foo", opt->with_foo, "Counter");
    sub->callback([opt]() {
        std::cout << "Working on file: " << opt->file << '\n';
        if (opt->with_foo) {
            std::cout << "Using foo!" << '\n';
        }
    });
}
