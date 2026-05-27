#include "breakout.h"

int main()
{
    Game::Breakout game;

    if (game.valid()) {
        game.run();
    }

    return 0;
}
