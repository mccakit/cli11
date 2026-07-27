import std;
import cli11;
import subcommand_a;

int main(int argc, char **argv) {
    cli::app_t app{"..."};
    setup_subcommand_a(app);
    app.require_subcommand();
    try {
        app.parse(argc, argv);
    } catch (const cli::parse_error_t &e) {
        return app.exit(e);
    }
    return 0;
}
