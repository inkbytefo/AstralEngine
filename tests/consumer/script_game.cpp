// A game with nothing to play: the installed SDK's astral_check_scripts() runs it with `--script-api-only` to write
// astral.d.luau, then type-checks tests/consumer/scripts against it with the installed astral-luau-check.
#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    struct Idle final : astral::Game {} game;
    return astral::run_game(options, {.title = "consumer scripts", .headless = true}, game, 1);
}
