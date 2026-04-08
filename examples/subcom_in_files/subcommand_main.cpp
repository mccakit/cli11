import std;
import cli11;
import subcommand_a;

int main(int argc, char **argv) {
    CLI::App app{"..."};
    setup_subcommand_a(app);
    app.require_subcommand();
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }
    return 0;
}
