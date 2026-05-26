#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

namespace Game
{
    struct Transform {
        SDL_FPoint p{};
        float angle = 0.0f;
    };

    struct Drawable {
        SDL_FPoint size{};
        SDL_Color color{};
    };

    struct Intent {
        float moveX = 0.0f;
        bool launch = false;
        bool restart = false;
    };

    struct Keys {
        SDL_Scancode left = SDL_SCANCODE_LEFT;
        SDL_Scancode right = SDL_SCANCODE_RIGHT;
        SDL_Scancode launch = SDL_SCANCODE_SPACE;
        SDL_Scancode restart = SDL_SCANCODE_R;
    };

    struct Collider {
        b2BodyId body = b2_nullBodyId;
        SDL_FPoint halfSize{};
        bool solid = true;
        bool active = true;
    };

    // Tag components
    struct BallTag {};
    struct PaddleTag {};
    struct BrickTag {};
    struct WallTag {};
    struct StateTag {};

    // Data components
    struct BrickData {
        int value = 0;
    };

    struct GameState {
        int score = 0;
        int lives = 3;
        int bricksLeft = 0;
        bool won = false;
        bool gameOver = false;
    };

    struct BallState {
        bool attached = true;
        int hitCounter = 0;
    };

    class Breakout
    {
    public:
        Breakout();
        ~Breakout();

        void run();
        bool valid() const;

    private:
        static constexpr int WIN_W = 800;
        static constexpr int WIN_H = 600;

        static constexpr int FPS = 60;
        static constexpr Uint64 GAME_FRAME = 1000 / FPS;

        static constexpr float BOX_SCALE = 10.0f;
        static constexpr float BOX_STEP = 1.0f / FPS;

        static constexpr float PADDLE_W = 110.0f;
        static constexpr float PADDLE_H = 18.0f;
        static constexpr float PADDLE_Y = 550.0f;
        static constexpr float PADDLE_SPEED = 720.0f;

        static constexpr float BALL_SIZE = 16.0f;
        static constexpr float BALL_SPEED_X = 320.0f;
        static constexpr float BALL_SPEED_Y = -380.0f;
        static constexpr float BALL_MIN_SPEED = 32.0f;

        static constexpr int BRICK_ROWS = 5;
        static constexpr int BRICK_COLS = 10;
        static constexpr float BRICK_W = 64.0f;
        static constexpr float BRICK_H = 24.0f;
        static constexpr float BRICK_GAP = 6.0f;
        static constexpr float BRICK_START_X = 53.0f;
        static constexpr float BRICK_START_Y = 70.0f;

        SDL_Window* win = nullptr;
        SDL_Renderer* ren = nullptr;
        b2WorldId box = b2_nullWorldId;

        bool initialized = false;

        void input_system() const;
        void paddle_system() const;
        void attached_ball_system() const;
        void box_system() const;
        void collision_system() const;
        void game_state_system();
        void draw_system() const;

        void create_entities();
        void create_walls();
        void create_bricks();
        void create_paddle();
        void create_ball();
        void create_state();

        b2BodyId create_body(float x, float y, float w, float h, b2BodyType type) const;

        template<class Tag>
        bagel::Entity find_first() const
        {
            for (auto e = bagel::Entity::first(); !e.eof(); e.next()) {
                if (e.has<Tag>()) {
                    return e;
                }
            }

            return bagel::Entity{{-1}};
        }

        void reset_ball_and_paddle() const;
        void lose_life() const;

        static SDL_Color brick_color(int row);
    };
}