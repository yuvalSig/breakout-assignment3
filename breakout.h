#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

/**
 * @namespace Game
 * @brief Contains the Breakout game components and main game class.
 */
namespace Game
{
    /**
     * @brief Stores the position and rotation of an entity.
     *
     * Used by rendering and by the Box2D synchronization system.
     */
    struct Transform {
        SDL_FPoint p{};
        float angle = 0.0f;
    };

    /**
     * @brief Stores visual rectangle size and color for rendering.
     */
    struct Drawable {
        SDL_FPoint size{};
        SDL_Color color{};
    };

    /**
     * @brief Stores high-level player intentions.
     *
     * Raw keyboard input is translated into this component by input_system().
     * Movement systems then use this component instead of reading the keyboard directly.
     */
    struct Intent {
        float moveX = 0.0f;
        bool launch = false;
        bool restart = false;
    };

    /**
     * @brief Stores configurable keyboard bindings for a controllable entity.
     */
    struct Keys {
        SDL_Scancode left = SDL_SCANCODE_LEFT;
        SDL_Scancode right = SDL_SCANCODE_RIGHT;
        SDL_Scancode launch = SDL_SCANCODE_SPACE;
        SDL_Scancode restart = SDL_SCANCODE_R;
    };

    /**
     * @brief Stores a Box2D body and collision data for an entity.
     *
     * The body field connects the ECS entity to the Box2D physics world.
     */
    struct Collider {
        b2BodyId body = b2_nullBodyId;
        SDL_FPoint halfSize{};
        bool solid = true;
        bool active = true;
    };

    /**
     * @brief Tag component marking an entity as the ball.
     */
    struct BallTag {};

    /**
     * @brief Tag component marking an entity as the player paddle.
     */
    struct PaddleTag {};

    /**
     * @brief Tag component marking an entity as a breakable brick.
     */
    struct BrickTag {};

    /**
     * @brief Tag component marking an entity as a wall.
     */
    struct WallTag {};

    /**
     * @brief Tag component marking the singleton game-state entity.
     */
    struct StateTag {};

    /**
     * @brief Stores data specific to a brick entity.
     */
    struct BrickData {
        int value = 0;
    };

    /**
     * @brief Stores global game state.
     *
     * This component is attached to a single state entity and stores score,
     * lives, remaining bricks, and end-game flags.
     */
    struct GameState {
        int score = 0;
        int lives = 3;
        int bricksLeft = 0;
        bool won = false;
        bool gameOver = false;
    };

    /**
     * @brief Stores runtime state specific to the ball entity.
     */
    struct BallState {
        bool attached = true;
        int hitCounter = 0;
    };

    /**
     * @brief Breakout demo implemented using the Bagel ECS engine.
     *
     * The game is built from ECS entities composed of small components such as
     * Transform, Drawable, Collider, Intent, and tag components. Systems process
     * entities by their component masks and communicate through components.
     *
     * Box2D is used for movement simulation and collision detection.
     *
     * @ref Transform
     * @ref Drawable
     * @ref Collider
     * @ref Intent
     * @ref BallTag
     * @ref PaddleTag
     * @ref BrickTag
     * @ref GameState
     */
    class Breakout
    {
    public:
        /**
         * @brief Initializes SDL, Box2D, and creates the ECS entities.
         */
        Breakout();

        /**
         * @brief Releases SDL and Box2D resources.
         */
        ~Breakout();

        /**
         * @brief Runs the main game loop.
         */
        void run();

        /**
         * @brief Checks whether the game initialized successfully.
         *
         * @return true if SDL and Box2D were initialized correctly, false otherwise.
         */
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

        /**
         * @brief Reads keyboard state and updates Intent components.
         *
         * This system does not move entities directly. It only translates raw
         * input into high-level intentions.
         *
         * @ref Intent
         * @ref Keys
         */
        void input_system() const;

        /**
         * @brief Moves the paddle according to its Intent component.
         *
         * The paddle movement is applied through its Box2D body and then clamped
         * to the screen boundaries.
         *
         * @ref PaddleTag
         * @ref Intent
         * @ref Collider
         */
        void paddle_system() const;

        /**
         * @brief Keeps the ball attached to the paddle before launch.
         *
         * When BallState::attached is true, the ball follows the paddle.
         * When launch is requested through Intent, the ball is released.
         *
         * @ref BallState
         * @ref BallTag
         * @ref PaddleTag
         */
        void attached_ball_system() const;

        /**
         * @brief Advances the Box2D world and synchronizes ECS transforms.
         *
         * Every entity with Transform and Collider is updated according to its
         * Box2D body position.
         *
         * @ref Transform
         * @ref Collider
         */
        void box_system() const;

        /**
         * @brief Handles ball contacts with bricks, paddle, and walls.
         *
         * This system reads Box2D contacts, destroys hit bricks, updates score
         * and brick counters, and corrects ball velocity when needed.
         *
         * @ref BallTag
         * @ref BrickTag
         * @ref PaddleTag
         * @ref GameState
         */
        void collision_system() const;

        /**
         * @brief Updates win, loss, lives, and game-over state.
         *
         * @ref GameState
         * @ref BallTag
         */
        void game_state_system();

        /**
         * @brief Draws all Drawable entities and end-game messages.
         *
         * @ref Drawable
         * @ref Transform
         * @ref GameState
         */
        void draw_system() const;

        /**
         * @brief Creates the initial game entities.
         */
        void create_entities();

        /**
         * @brief Creates the static wall entities.
         */
        void create_walls();

        /**
         * @brief Creates the brick grid.
         */
        void create_bricks();

        /**
         * @brief Creates the player paddle entity.
         */
        void create_paddle();

        /**
         * @brief Creates the ball entity.
         */
        void create_ball();

        /**
         * @brief Creates the singleton game-state entity.
         */
        void create_state();

        /**
         * @brief Creates a rectangular Box2D body.
         *
         * @param x Center x position in pixels.
         * @param y Center y position in pixels.
         * @param w Width in pixels.
         * @param h Height in pixels.
         * @param type Box2D body type.
         * @return The created Box2D body id.
         */
        b2BodyId create_body(float x, float y, float w, float h, b2BodyType type) const;

        /**
         * @brief Finds the first entity that has the requested tag component.
         *
         * @tparam Tag Component type to search for.
         * @return First matching entity, or an invalid entity if none was found.
         */
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

        /**
         * @brief Resets the paddle and ball positions after a lost life.
         */
        void reset_ball_and_paddle() const;

        /**
         * @brief Decreases the life counter and resets the round.
         */
        void lose_life() const;

        /**
         * @brief Returns a brick color according to its row.
         *
         * @param row Brick row index.
         * @return SDL color for the requested row.
         */
        static SDL_Color brick_color(int row);
    };
}