#include "breakout.h"

/**
 * @brief Program entry point.
 *
 * Creates and runs the Breakout ECS demo.
 *
 * @return 0 on normal program termination.
 */
int main()
{
    Game::Breakout game;

    if (game.valid()) {
        game.run();
    }

    return 0;
}