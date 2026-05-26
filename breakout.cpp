#include "breakout.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <random>
using namespace bagel;

namespace Game
{
    bool Breakout::valid() const
    {
        return initialized && b2World_IsValid(box);
    }

    Breakout::Breakout()
    {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cout << SDL_GetError() << std::endl;
            return;
        }

        if (!SDL_CreateWindowAndRenderer("Breakout ECS", WIN_W, WIN_H, 0, &win, &ren)) {
            std::cout << SDL_GetError() << std::endl;
            return;
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = {0.0f, 0.0f};

        box = b2CreateWorld(&worldDef);
        if (!b2World_IsValid(box)) {
            std::cout << "Failed creating Box2D world" << std::endl;
            return;
        }

        create_entities();
        initialized = true;
    }

    Breakout::~Breakout()
    {
        if (b2World_IsValid(box)) {
            b2DestroyWorld(box);
        }

        if (ren != nullptr) {
            SDL_DestroyRenderer(ren);
        }

        if (win != nullptr) {
            SDL_DestroyWindow(win);
        }

        SDL_Quit();
    }

    void Breakout::run()
    {
        Uint64 start = SDL_GetTicks();
        bool quit = false;

        while (!quit) {
            input_system();
            paddle_system();
            attached_ball_system();
            box_system();
            collision_system();
            game_state_system();
            Entity state = find_first<StateTag>();

            if (!state.eof() &&
                state.get<GameState>().gameOver)
            {
                SDL_Delay(1500);
                quit = true;
            }

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            draw_system();
            SDL_RenderPresent(ren);

            const Uint64 end = SDL_GetTicks();
            if (end - start < GAME_FRAME) {
                SDL_Delay(GAME_FRAME - (end - start));
            }
            start += GAME_FRAME;

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.scancode == SDL_SCANCODE_ESCAPE)) {
                    quit = true;
                }
            }
        }
    }

    void Breakout::create_entities()
    {
        create_state();
        create_walls();
        create_bricks();
        create_paddle();
        create_ball();
    }

    void Breakout::create_state()
    {
        Entity::create().addAll(
            StateTag{},
            GameState{
                0,
                3,
                BRICK_ROWS * BRICK_COLS,
                false,
                false
            }
        );
    }

    void Breakout::create_walls()
    {
        const SDL_Color wallColor{180, 180, 180, 255};

        Entity::create().addAll(
            Transform{{static_cast<float>(WIN_W) / 2.0f, 15.0f}, 0.0f},
            Drawable{{static_cast<float>(WIN_W), 30.0f}, wallColor},
            Collider{
                create_body(static_cast<float>(WIN_W) / 2.0f, 15.0f, static_cast<float>(WIN_W), 30.0f, b2_staticBody),
                {static_cast<float>(WIN_W) / 2.0f, 15.0f},
                true,
                true
            },
            WallTag{}
        );

        Entity::create().addAll(
            Transform{{15.0f, static_cast<float>(WIN_H) / 2.0f}, 0.0f},
            Drawable{{30.0f, static_cast<float>(WIN_H)}, wallColor},
            Collider{
                create_body(15.0f, WIN_H / 2.0f, 30.0f, static_cast<float>(WIN_H), b2_staticBody),
                {15.0f, static_cast<float>(WIN_H) / 2.0f},
                true,
                true
            },
            WallTag{}
        );

        Entity::create().addAll(
        Transform{
{
static_cast<float>(WIN_W) - 15.0f,
static_cast<float>(WIN_H) / 2.0f
},
0.0f
},
            Drawable{{30.0f, static_cast<float>(WIN_H)}, wallColor},
            Collider{
                create_body(static_cast<float>(WIN_W) - 15.0f, static_cast<float>(WIN_H) / 2.0f, 30.0f, static_cast<float>(WIN_H), b2_staticBody),
                {15.0f, static_cast<float>(WIN_H) / 2.0f},
                true,
                true
            },
            WallTag{}
        );
    }

    void Breakout::create_bricks()
    {
        for (int row = 0; row < BRICK_ROWS; ++row) {
            for (int col = 0; col < BRICK_COLS; ++col) {
                const float x = BRICK_START_X + col * (BRICK_W + BRICK_GAP) + BRICK_W / 2.0f;
                const float y = BRICK_START_Y + row * (BRICK_H + BRICK_GAP) + BRICK_H / 2.0f;

                Entity::create().addAll(
                    Transform{{x, y}, 0.0f},
                    Drawable{{BRICK_W, BRICK_H}, brick_color(row)},
                    Collider{
                        create_body(x, y, BRICK_W, BRICK_H, b2_staticBody),
                        {BRICK_W / 2.0f, BRICK_H / 2.0f},
                        true,
                        true
                    },
                    BrickTag{},
                    BrickData{(BRICK_ROWS - row) * 10}
                );
            }
        }
    }

    void Breakout::create_paddle()
    {
        const b2BodyId body = create_body(
            static_cast<float>(WIN_W) / 2.0f,
            PADDLE_Y,
            PADDLE_W,
            PADDLE_H,
            b2_kinematicBody
        );

        Entity::create().addAll(
            Transform{{static_cast<float>(WIN_W) / 2.0f, PADDLE_Y}, 0.0f},
            Drawable{{PADDLE_W, PADDLE_H}, SDL_Color{80, 180, 255, 255}},
            Collider{body, {PADDLE_W / 2.0f, PADDLE_H / 2.0f}, true, true},
            Intent{},
            Keys{SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_SPACE, SDL_SCANCODE_R},
            PaddleTag{}
        );
    }

    void Breakout::create_ball()
    {
        const float ballY = PADDLE_Y - PADDLE_H / 2.0f - BALL_SIZE;

        const b2BodyId body = create_body(
            static_cast<float>(WIN_W) / 2.0f,
            ballY,
            BALL_SIZE,
            BALL_SIZE,
            b2_dynamicBody
        );

        static std::mt19937 rng{
            std::random_device{}()
        };

        std::uniform_real_distribution<float> dist(
            -1.0f,
            1.0f
        );

        float randomDir = dist(rng);

        if (std::fabs(randomDir) < 0.25f) {
            randomDir =
                randomDir < 0.0f ?
                -0.25f :
                0.25f;
        }

        b2Body_SetLinearVelocity(
            body,
            {
                randomDir * BALL_SPEED_X / BOX_SCALE,
                BALL_SPEED_Y / BOX_SCALE
            }
        );

        Entity::create().addAll(
            Transform{{static_cast<float>(WIN_W) / 2.0f, ballY}, 0.0f},
            Drawable{{BALL_SIZE, BALL_SIZE}, SDL_Color{255, 255, 255, 255}},
            Collider{body, {BALL_SIZE / 2.0f, BALL_SIZE / 2.0f}, true, true},
            BallTag{},
            BallState{false, 0}
        );
    }

    b2BodyId Breakout::create_body(float x, float y, float w, float h, b2BodyType type) const
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = type;
        bodyDef.position = {x / BOX_SCALE, y / BOX_SCALE};

        b2BodyId body = b2CreateBody(box, &bodyDef);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.material.friction = 0.0f;
        shapeDef.material.restitution = 1.0f;

        b2Polygon poly = b2MakeBox(
            w / BOX_SCALE / 2.0f,
            h / BOX_SCALE / 2.0f
        );

        b2CreatePolygonShape(body, &shapeDef, &poly);
        return body;
    }
    void Breakout::input_system() const
    {
        static const Mask mask =
            MaskBuilder()
                .set<Keys>()
                .set<Intent>()
                .build();

        SDL_PumpEvents();

        const bool* keyboard =
            SDL_GetKeyboardState(nullptr);

        for (Entity e = Entity::first();
             !e.eof();
             e.next())
        {
            if (!e.test(mask)) {
                continue;
            }

            const auto& keys =
                e.get<Keys>();

            auto& intent =
                e.get<Intent>();

            intent.moveX = 0.0f;

            if (keyboard[keys.left]) {
                intent.moveX -= 1.0f;
            }

            if (keyboard[keys.right]) {
                intent.moveX += 1.0f;
            }

            intent.launch =
                keyboard[keys.launch];

            intent.restart =
                keyboard[keys.restart];
        }
    }

    void Breakout::paddle_system() const
    {
        static const Mask mask =
            MaskBuilder()
                .set<Transform>()
                .set<Collider>()
                .set<Intent>()
                .set<PaddleTag>()
                .build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;



            auto& transform = e.get<Transform>();
            const auto& collider = e.get<Collider>();
            const auto& intent = e.get<Intent>();

            b2Body_SetLinearVelocity(
                collider.body,
                {intent.moveX * PADDLE_SPEED / BOX_SCALE, 0.0f}
            );

            b2Vec2 pos = b2Body_GetPosition(collider.body);

            float x = pos.x * BOX_SCALE;
            float y = pos.y * BOX_SCALE;

            const float minX = 30.0f + PADDLE_W / 2.0f;
            const float maxX = WIN_W - 30.0f - PADDLE_W / 2.0f;

            if (x < minX) {
                x = minX;
                b2Body_SetLinearVelocity(collider.body, {0.0f, 0.0f});
            }

            if (x > maxX) {
                x = maxX;
                b2Body_SetLinearVelocity(collider.body, {0.0f, 0.0f});
            }

            transform.p = {x, y};

            b2Body_SetTransform(
                collider.body,
                {x / BOX_SCALE, PADDLE_Y / BOX_SCALE},
                b2MakeRot(0.0f)
            );
        }
    }

    void Breakout::attached_ball_system() const
    {
        Entity ball =
            find_first<BallTag>();

        Entity paddle =
            find_first<PaddleTag>();



        if (ball.eof() || paddle.eof()) {
            return;
        }

        auto& ballState =
            ball.get<BallState>();

        if (!ballState.attached) {
            return;
        }

        const auto& paddlePos =
            paddle.get<Transform>();

        auto& ballPos =
            ball.get<Transform>();

        const auto& body =
            ball.get<Collider>();

        ballPos.p = {
            paddlePos.p.x,
            PADDLE_Y -
                PADDLE_H / 2 -
                BALL_SIZE
        };

        b2Body_SetTransform(
            body.body,
            {
                ballPos.p.x /
                    BOX_SCALE,

                ballPos.p.y /
                    BOX_SCALE
            },
            b2MakeRot(0)
        );

        if (paddle.get<Intent>().launch)
        {
            ballState.attached = false;

            static std::mt19937 rng{
                std::random_device{}()
            };

            std::uniform_real_distribution<float> dist(
                -1.0f,
                1.0f
            );

            float randomDir = dist(rng);

            if (std::fabs(randomDir) < 0.25f) {
                randomDir =
                    randomDir < 0.0f ?
                    -0.25f :
                    0.25f;
            }

            b2Body_SetLinearVelocity(
                body.body,
                {
                    randomDir *
                        BALL_SPEED_X /
                        BOX_SCALE,

                    BALL_SPEED_Y /
                        BOX_SCALE
                }
            );
        }
    }

    void Breakout::box_system() const
    {
        static const Mask mask =
            MaskBuilder()
                .set<Transform>()
                .set<Collider>()
                .build();

        b2World_Step(
            box,
            BOX_STEP,
            4
        );

        for (Entity e = Entity::first();
             !e.eof();
             e.next())
        {
            if (!e.test(mask)) {
                continue;
            }

            const auto& collider =
                e.get<Collider>();

            if (!b2Body_IsValid(
                    collider.body))
            {
                continue;
            }

            const auto tr =
                b2Body_GetTransform(
                    collider.body
                );

            e.get<Transform>().p =
            {
                tr.p.x *
                    BOX_SCALE,

                tr.p.y *
                    BOX_SCALE
            };
        }
    }
    static bool same_body(b2BodyId a, b2BodyId b)
    {
        return a.index1 == b.index1 &&
               a.world0 == b.world0 &&
               a.generation == b.generation;
    }

    static Entity find_entity_by_body(b2BodyId body)
    {
        static const Mask mask =
            MaskBuilder()
                .set<Collider>()
                .build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) {
                continue;
            }

            const auto& collider = e.get<Collider>();

            if (b2Body_IsValid(collider.body) &&
                same_body(collider.body, body))
            {
                return e;
            }
        }

        return Entity{{World::maxId() + 1}};
    }
    void Breakout::collision_system() const
    {
        Entity ball = find_first<BallTag>();
        Entity state = find_first<StateTag>();

    if (ball.eof() || state.eof()) {
        return;
    }

    auto& ballState = ball.get<BallState>();
    const auto& ballCollider = ball.get<Collider>();
    auto& gameState = state.get<GameState>();

        if (ballState.attached ||
            gameState.gameOver ||
            gameState.won)
    {
        return;
    }

    constexpr int MAX_CONTACTS = 16;
    b2ContactData contacts[MAX_CONTACTS];

    const int capacity =
        b2Body_GetContactCapacity(ballCollider.body);

    if (capacity <= 0) {
        return;
    }

    const int count =
        b2Body_GetContactData(
            ballCollider.body,
            contacts,
            std::min(MAX_CONTACTS, capacity)
        );

    for (int i = 0; i < count; ++i) {
        const b2ContactData& contact = contacts[i];

        if (contact.manifold.pointCount == 0) {
            continue;
        }

        const b2BodyId bodyA =
            b2Shape_GetBody(contact.shapeIdA);

        const b2BodyId bodyB =
            b2Shape_GetBody(contact.shapeIdB);

        const b2BodyId otherBody =
            same_body(bodyA, ballCollider.body)
                ? bodyB
                : bodyA;

        Entity other =
            find_entity_by_body(otherBody);

        if (other.eof()) {
            continue;
        }

        ++ballState.hitCounter;

        if (other.has<BrickTag>()) {
            gameState.score += other.get<BrickData>().value;
            --gameState.bricksLeft;

            const auto& brickCollider =
                other.get<Collider>();

            if (b2Body_IsValid(brickCollider.body)) {
                b2DestroyBody(brickCollider.body);
            }

            other.destroy();
            break;
        }

        if (other.has<PaddleTag>()) {
            const float ballX =
                ball.get<Transform>().p.x;

            const float paddleX =
                other.get<Transform>().p.x;

            const float normalized =
                (ballX - paddleX) /
                (PADDLE_W / 2.0f);

            b2Vec2 velocity =
                b2Body_GetLinearVelocity(
                    ballCollider.body
                );

            velocity.x =
                normalized *
                BALL_SPEED_X /
                BOX_SCALE;

            velocity.y =
                -std::fabs(velocity.y);

            b2Body_SetLinearVelocity(
                ballCollider.body,
                velocity
            );

            break;
        }
    }
        b2Vec2 velocity =
    b2Body_GetLinearVelocity(ballCollider.body);

        const float speed =
            std::sqrt(
                velocity.x * velocity.x +
                velocity.y * velocity.y
            );

        if (speed < BALL_MIN_SPEED && speed > 0.001f) {
            const float factor =
                BALL_MIN_SPEED / speed;

            velocity.x *= factor;
            velocity.y *= factor;

            b2Body_SetLinearVelocity(
                ballCollider.body,
                velocity
            );
        }
    }

    void Breakout::draw_system() const
    {
        static const Mask mask =
            MaskBuilder()
                .set<Transform>()
                .set<Drawable>()
                .build();

        for (Entity e = Entity::first();
             !e.eof();
             e.next())
        {
            if (!e.test(mask)) {
                continue;
            }

            const auto& transform =
                e.get<Transform>();

            const auto& drawable =
                e.get<Drawable>();

            SDL_FRect rect{
                transform.p.x -
                    drawable.size.x / 2.0f,

                transform.p.y -
                    drawable.size.y / 2.0f,

                drawable.size.x,
                drawable.size.y
            };

            SDL_SetRenderDrawColor(
                ren,
                drawable.color.r,
                drawable.color.g,
                drawable.color.b,
                drawable.color.a
            );

            SDL_RenderFillRect(
                ren,
                &rect
            );
        }
        Entity state = find_first<StateTag>();

        if (!state.eof()) {
            const auto& gameState = state.get<GameState>();

            if (gameState.won) {
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);

                SDL_FRect banner{
                    220.0f,
                    250.0f,
                    360.0f,
                    90.0f
                };

                SDL_RenderFillRect(ren, &banner);

                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDebugText(ren, 350.0f, 290.0f, "YOU WIN");
            }

            if (gameState.gameOver) {
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);

                SDL_FRect banner{
                    210.0f,
                    250.0f,
                    380.0f,
                    90.0f
                };

                SDL_RenderFillRect(ren, &banner);

                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDebugText(ren, 340.0f, 290.0f, "GAME OVER");
            }
        }
    }

    void Breakout::reset_ball_and_paddle() const
    {
        Entity paddle =
            find_first<PaddleTag>();

        Entity ball =
            find_first<BallTag>();

        if (paddle.eof() ||
            ball.eof())
        {
            return;
        }

        auto& paddleTransform =
            paddle.get<Transform>();

        const auto& paddleCollider =
            paddle.get<Collider>();

        paddleTransform.p =
        {
            static_cast<float>(WIN_W) / 2.0f,
            PADDLE_Y
        };

        b2Body_SetTransform(
            paddleCollider.body,
            {
                paddleTransform.p.x /
                    BOX_SCALE,

                paddleTransform.p.y /
                    BOX_SCALE
            },
            b2MakeRot(0.0f)
        );

        b2Body_SetLinearVelocity(
            paddleCollider.body,
            {0.0f, 0.0f}
        );

        auto& ballTransform =
            ball.get<Transform>();

        auto& ballState = ball.get<BallState>();

        const auto& ballCollider =
            ball.get<Collider>();

        ballState.attached = false;

        ballTransform.p =
        {
            static_cast<float>(WIN_W) / 2.0f,
            PADDLE_Y -
                PADDLE_H / 2.0f -
                BALL_SIZE
        };

        b2Body_SetTransform(
            ballCollider.body,
            {
                ballTransform.p.x /
                    BOX_SCALE,

                ballTransform.p.y /
                    BOX_SCALE
            },
            b2MakeRot(0.0f)
        );

        static std::mt19937 rng{
            std::random_device{}()
        };

        std::uniform_real_distribution<float> dist(
            -1.0f,
            1.0f
        );

        float randomDir = dist(rng);

        if (std::fabs(randomDir) < 0.25f) {
            randomDir =
                randomDir < 0.0f ?
                -0.25f :
                0.25f;
        }

        b2Body_SetLinearVelocity(
            ballCollider.body,
            {
                randomDir * BALL_SPEED_X / BOX_SCALE,
                BALL_SPEED_Y / BOX_SCALE
            }
        );
    }


    void Breakout::game_state_system()
    {
        Entity state =
            find_first<StateTag>();

        Entity ball =
            find_first<BallTag>();

        if (state.eof() ||
            ball.eof())
        {
            return;
        }

        auto& gameState =
            state.get<GameState>();

        if (gameState.bricksLeft <= 0) {
            gameState.won = true;

            const auto& ballCollider =
                ball.get<Collider>();

            b2Body_SetLinearVelocity(
                ballCollider.body,
                {0.0f, 0.0f}
            );
        }

        const auto& ballTransform =
            ball.get<Transform>();

        if (ballTransform.p.y > static_cast<float>(WIN_H)) {
            lose_life();

            if (gameState.lives <= 0) {
                gameState.gameOver = true;
            }
        }
    }

    void Breakout::lose_life() const
    {
        Entity state =
            find_first<StateTag>();

        if (state.eof()) {
            return;
        }

        auto& gameState =
            state.get<GameState>();

        --gameState.lives;

        reset_ball_and_paddle();
    }

    SDL_Color Breakout::brick_color(
        int row
    )
    {
        switch (row) {
            case 0:
                return SDL_Color{
                    220, 60, 60, 255
                };

            case 1:
                return SDL_Color{
                    235, 150, 60, 255
                };

            case 2:
                return SDL_Color{
                    235, 220, 70, 255
                };

            case 3:
                return SDL_Color{
                    80, 200, 120, 255
                };

            default:
                return SDL_Color{
                    80, 140, 240, 255
                };
        }
    }
}