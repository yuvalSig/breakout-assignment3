#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

/**
 * @namespace Game
 * @brief Contains the Breakout game components and main game class.
 */
namespace Game {

    // ── Components ────────────────────────────────────────────────────────────

    /**
     * @brief Stores the position and rotation of an entity.
     *
     * Used by rendering and by the Box2D synchronisation system.
     */
    struct Transform {
        SDL_FPoint p{};       ///< 2D position in screen pixels.
        float angle = 0.0f;  ///< Rotation angle in degrees.
    };

    /**
     * @brief Stores the spritesheet section and on-screen size used for rendering.
     */
    struct Drawable {
        SDL_FRect  part{};  ///< Rectangle inside the spritesheet texture.
        SDL_FPoint size{};  ///< Size in pixels when drawn on screen.
    };

    /**
     * @brief Stores high-level player intentions.
     *
     * Raw keyboard input is translated into this component by input_system().
     * Movement systems then read this component instead of the keyboard directly.
     */
    struct Intent {
        float moveX  = 0.0f;  ///< Horizontal movement direction: -1 left, +1 right, 0 idle.
        bool  launch  = false; ///< True while the launch key is held.
        bool  restart = false; ///< True while the restart key is held.
    };

    /**
     * @brief Stores configurable keyboard bindings for a controllable entity.
     */
    struct Keys {
        SDL_Scancode left    = SDL_SCANCODE_LEFT;  ///< Move paddle left.
        SDL_Scancode right   = SDL_SCANCODE_RIGHT; ///< Move paddle right.
        SDL_Scancode launch  = SDL_SCANCODE_SPACE; ///< Launch the ball.
        SDL_Scancode restart = SDL_SCANCODE_R;     ///< Restart the game.
    };

    /**
     * @brief Stores a Box2D body and collision data for an entity.
     *
     * The body field connects the ECS entity to the Box2D physics world.
     */
    struct Collider {
        b2BodyId   body     = b2_nullBodyId; ///< Handle to the Box2D rigid body.
        SDL_FPoint halfSize{};               ///< Half-extents of the collision shape in pixels.
        bool       solid  = true;            ///< True for solid bodies, false for sensors.
        bool       active = true;            ///< Whether this collider participates in physics.
    };

    /**
     * @brief Stores runtime state specific to the ball entity.
     */
    struct BallState {
        bool attached   = true; ///< True while the ball is glued to the paddle before launch.
        int  hitCounter = 0;    ///< Number of bricks hit since the last launch.
    };

    /**
     * @brief Stores data specific to a brick entity.
     */
    struct BrickData {
        int value = 0; ///< Score awarded when this brick is destroyed.
    };

    /**
     * @brief Stores global game state.
     *
     * Attached to a single state entity. Updated by collision_system()
     * and read by draw_system() and the end-game check in run().
     */
    struct GameState {
        int  score      = 0;     ///< Accumulated player score.
        int  lives      = 3;     ///< Remaining lives.
        int  bricksLeft = 0;     ///< Number of bricks not yet destroyed.
        bool won        = false;  ///< Set to true when all bricks are cleared.
        bool gameOver   = false;  ///< Set to true when all lives are lost.
    };

    // ── Tag components ────────────────────────────────────────────────────────

    /// @brief Tag component marking the ball entity.
    struct BallTag {};
    /// @brief Tag component marking the player paddle entity.
    struct PaddleTag {};
    /// @brief Tag component marking a breakable brick entity.
    struct BrickTag {};
    /// @brief Tag component marking a static wall entity.
    struct WallTag {};
    /// @brief Tag component marking the bottom sensor that detects missed balls.
    struct DeathZoneTag {};
    /// @brief Tag component marking the singleton game-state entity.
    struct StateTag {};
    /// @brief Tag component marking a life-heart entity.
    struct HeartTag {};

    // ── Storage overrides (packed for frequently iterated components) ─────────

} // namespace Game

/// @brief Use PackedStorage for Intent so iteration is cache-friendly.
template <> struct bagel::Storage<Game::Intent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<Game::Intent>;
};
/// @brief Use PackedStorage for Keys so iteration is cache-friendly.
template <> struct bagel::Storage<Game::Keys> final : bagel::NoInstance {
    using type = bagel::PackedStorage<Game::Keys>;
};

namespace Game {

    /**
     * @brief Breakout demo implemented using the Bagel ECS engine.
     *
     * Breakout owns the SDL window, renderer, spritesheet texture, and Box2D
     * world. It creates the ECS entities that make up the game level and runs
     * the frame loop that updates input, physics, collision handling, and rendering.
     *
     * All game logic lives in small, independent systems that iterate over
     * entities by component mask, following the same pattern as the lecturer's
     * Pong implementation. No entity is searched by identity; every system
     * operates on whichever entities match its mask.
     *
     * Box2D sensors are used for brick hits and the death zone below the paddle.
     * Direct contact data is used for paddle deflection.
     *
     * @invariant valid() is true only after SDL resources, the spritesheet
     *            texture, and the Box2D world were created successfully.
     * @invariant Each entity with Collider is expected to also have Transform
     *            so Box2D positions can be synchronised into ECS state.
     * @invariant Each rendered entity is expected to have both Transform and
     *            Drawable components.
     */
    class Breakout
    {
    public:
        /**
         * @brief Initialises SDL, Box2D, and creates all ECS entities.
         *
         * Construction may fail without throwing. Call valid() before running
         * the game loop.
         */
        Breakout();

        /**
         * @brief Releases all SDL and Box2D resources.
         *
         * Destroys the Box2D world, texture, renderer, and window if they were
         * created successfully.
         */
        ~Breakout();

        /**
         * @brief Runs the main game loop.
         *
         * Calls all systems in order each frame: input, paddle, attached-ball,
         * physics, collision, then draw. Terminates when the player quits or
         * GameState signals game-over or victory.
         */
        void run();

        /**
         * @brief Checks whether the game initialised successfully.
         *
         * @return true if SDL and Box2D were initialised correctly, false otherwise.
         */
        bool valid() const;

    private:
        // ── Window / renderer constants ───────────────────────────────────────

        static constexpr int    WIN_W      = 800;        ///< Window width in pixels.
        static constexpr int    WIN_H      = 600;        ///< Window height in pixels.
        static constexpr int    FPS        = 60;         ///< Target frames per second.
        static constexpr Uint64 GAME_FRAME = 1000 / FPS; ///< Duration of one frame in ms.

        // ── Scale constants ───────────────────────────────────────────────────

        static constexpr float TEX_SCALE = 0.25f;        ///< Sprite scale factor applied to texture coordinates.
        static constexpr float BOX_SCALE = 10.0f;        ///< Pixels per Box2D metre.
        static constexpr float BOX_STEP  = 1.0f / FPS;   ///< Physics time step in seconds.

        // ── Paddle constants ──────────────────────────────────────────────────

        static constexpr float PADDLE_W     = 110.0f;    ///< Paddle width in pixels.
        static constexpr float PADDLE_H     = 18.0f;     ///< Paddle height in pixels.
        static constexpr float PADDLE_Y     = 550.0f;    ///< Fixed vertical position of the paddle.
        static constexpr float PADDLE_SPEED = 720.0f;    ///< Paddle movement speed in pixels/second.

        // ── Ball constants ────────────────────────────────────────────────────

        static constexpr float BALL_SIZE      = 16.0f;   ///< Ball width and height in pixels.
        static constexpr float BALL_SPEED_X   = 320.0f;  ///< Horizontal launch speed in pixels/second.
        static constexpr float BALL_SPEED_Y   = -380.0f; ///< Vertical launch speed in pixels/second (upward).
        static constexpr float BALL_MIN_SPEED = 32.0f;   ///< Minimum ball speed enforced after every contact.

        // ── Brick grid constants ──────────────────────────────────────────────

        static constexpr int   BRICK_ROWS    = 5;        ///< Number of brick rows.
        static constexpr int   BRICK_COLS    = 10;       ///< Number of brick columns.
        static constexpr float BRICK_W       = 64.0f;    ///< Brick width in pixels.
        static constexpr float BRICK_H       = 24.0f;    ///< Brick height in pixels.
        static constexpr float BRICK_GAP     = 6.0f;     ///< Gap between adjacent bricks in pixels.
        static constexpr float BRICK_START_X = 53.0f;    ///< X position of the first brick's left edge.
        static constexpr float BRICK_START_Y = 70.0f;    ///< Y position of the first brick's top edge.

        // ── Sprite rects inside the spritesheet ───────────────────────────────

        static constexpr SDL_FRect WALL_SPRITE   {  20.0f,  17.0f, 170.0f, 56.0f }; ///< Wall sprite in the spritesheet.
        static constexpr SDL_FRect PADDLE_SPRITE {  20.0f, 396.0f, 170.0f, 56.0f }; ///< Paddle sprite in the spritesheet.
        static constexpr SDL_FRect BALL_SPRITE   { 806.0f, 550.0f,  72.0f, 72.0f }; ///< Ball sprite in the spritesheet.
        static constexpr SDL_FRect HEART_SPRITE  { 800.0f, 455.0f,  90.0f, 80.0f };  ///< Heart/life sprite in the spritesheet.

        // ── SDL / Box2D handles ───────────────────────────────────────────────

        SDL_Texture*  tex  = nullptr;          ///< Spritesheet texture.
        SDL_Window*   win  = nullptr;          ///< SDL window.
        SDL_Renderer* ren  = nullptr;          ///< SDL renderer.
        b2WorldId     box  = b2_nullWorldId;   ///< Box2D physics world.
        bool initialized   = false;            ///< True after successful construction.

        // ── Systems ───────────────────────────────────────────────────────────

        /**
         * @brief Reads keyboard state and updates Intent components.
         *
         * Iterates entities with Keys + Intent mask. Translates raw key state
         * into directional and action intentions. Does not move any entity directly.
         */
        void input_system() const;

        /**
         * @brief Moves entities that have Intent + PaddleTag according to their Intent component.
         *
         * Iterates entities with Transform + Collider + Intent + PaddleTag mask.
         * Sets Box2D linear velocity and clamps the paddle within the playfield.
         */
        void move_system() const;

        /**
         * @brief Keeps the ball glued to the paddle before launch.
         *
         * Iterates BallState + BallTag entities and PaddleTag + Intent entities.
         * While BallState::attached is true the ball follows the paddle position.
         * When the launch intent is detected the ball is released with a random
         * horizontal direction.
         */
        void attached_ball_system() const;

        /**
         * @brief Advances the Box2D simulation and synchronises ECS transforms.
         *
         * Iterates entities with Transform + Collider mask. Steps the physics
         * world by BOX_STEP seconds, then copies each body's position back into
         * the corresponding Transform component.
         */
        void box_system() const;

        /**
         * @brief Handles all sensor events and paddle contacts.
         *
         * Processes Box2D sensor begin-events each frame:
         * - DeathZoneTag sensor: decrements lives in GameState; resets ball and paddle.
         * - BrickTag sensor: awards score, decrements bricksLeft, redirects ball,
         *   destroys the brick body and ECS entity. Sets GameState::won when all
         *   bricks are cleared.
         *
         * Also reads direct contact data on the ball body to redirect it when it
         * touches the paddle, and enforces BALL_MIN_SPEED after every contact.
         * GameState is updated directly through its mask — no entity is searched
         * by identity.
         */
        void collision_system() const;

        /**
         * @brief Draws all Drawable entities and overlays end-game messages.
         *
         * Iterates entities with Transform + Drawable mask and renders each sprite.
         * Then iterates the StateTag + GameState mask and, if the game has ended,
         * draws a centred "YOU WIN" or "GAME OVER" banner.
         */
        void draw_system() const;

        // ── Entity creation helpers ───────────────────────────────────────────

        /// @brief Creates all initial game entities.
        void create_entities();

        /// @brief Creates the three static wall entities and the bottom death-zone sensor.
        void create_walls();

        /// @brief Creates the full BRICK_ROWS × BRICK_COLS brick grid.
        void create_bricks();

        /// @brief Creates the player paddle entity.
        void create_paddle();

        /// @brief Creates the ball entity and gives it an initial random velocity.
        void create_ball();

        /// @brief Creates the singleton game-state entity.
        void create_state();

        /// @brief Creates the heart entities representing starting lives.
        void create_hearts();

        /**
         * @brief Builds a Drawable from a spritesheet rectangle and a screen size.
         *
         * @param part Rectangle inside the spritesheet.
         * @param size Size used when drawing the sprite on screen.
         * @return Drawable component configured for texture rendering.
         */
        static constexpr Drawable make_drawable(SDL_FRect part, SDL_FPoint size);

        /**
         * @brief Returns the spritesheet rectangle for a given brick row.
         *
         * @param row Brick row index (0 = top row, highest point value).
         * @return SDL_FRect covering the correct brick sprite.
         */
        static SDL_FRect brick_sprite(int row);

        // ── Box2D body helpers ────────────────────────────────────────────────

        /**
         * @brief Creates a rectangular Box2D body.
         *
         * @param x                  Centre x position in pixels.
         * @param y                  Centre y position in pixels.
         * @param w                  Width in pixels.
         * @param h                  Height in pixels.
         * @param type               Box2D body type (static, kinematic, dynamic).
         * @param enableSensorEvents True when the body should report sensor events.
         * @return The created Box2D body id.
         */
        b2BodyId create_body(float x, float y, float w, float h,
                             b2BodyType type,
                             bool enableSensorEvents = false) const;

        /**
         * @brief Creates a static rectangular Box2D sensor body.
         *
         * @param x Centre x position in pixels.
         * @param y Centre y position in pixels.
         * @param w Width in pixels.
         * @param h Height in pixels.
         * @return The created sensor body id.
         */
        b2BodyId create_sensor(float x, float y, float w, float h) const;

        // ── Round-reset helpers ───────────────────────────────────────────────

        /**
         * @brief Repositions the ball to the centre of the screen and gives it
         *        a new random horizontal launch velocity.
         *
         * Called by collision_system() when the ball enters the death zone.
         *
         * @param ballBody The Box2D body of the ball entity.
         */
        void reset_ball(b2BodyId ballBody) const;

        /**
         * @brief Recentres the paddle and zeroes its velocity.
         *
         * Iterates entities with Collider + PaddleTag mask.
         * Called by collision_system() after a life is lost.
         */
        void reset_paddle() const;

        // ── collision_system helpers ──────────────────────────────────────────

        /**
         * @brief Decrements lives, resets ball and paddle after the ball enters the death zone.
         * @param ballEnt The ball entity that triggered the event.
         */
        void on_death_zone(bagel::Entity ballEnt) const;

        /**
         * @brief Awards score, redirects ball, and destroys the brick on a brick hit.
         * @param ballEnt  The ball entity that hit the brick.
         * @param brickEnt The brick entity that was hit.
         */
        void on_brick_hit(bagel::Entity ballEnt, bagel::Entity brickEnt) const;

        /**
         * @brief Redirects the ball on paddle contact and enforces minimum speed.
         * @param ballEnt The ball entity whose contacts are checked.
         */
        void on_paddle_contact(bagel::Entity ballEnt) const;
    };

} // namespace Game