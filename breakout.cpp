#include "breakout.h"

#include <SDL3_image/SDL_image.h>
#include <iostream>
using namespace bagel;

namespace Game
{
    // ─────────────────────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────────────────────

    constexpr Drawable Breakout::make_drawable(SDL_FRect part, SDL_FPoint size)
    {
        return Drawable{part, size};
    }

    SDL_FRect Breakout::brick_sprite(int row)
    {
        switch (row) {
            case 0:  return {  20.0f, 244.0f, 170.0f, 56.0f };
            case 1:  return {  20.0f, 319.0f, 170.0f, 56.0f };
            case 2:  return {  20.0f, 470.0f, 170.0f, 56.0f };
            case 3:  return {  20.0f,  93.0f, 170.0f, 56.0f };
            default: return {  20.0f,  17.0f, 170.0f, 56.0f };
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Construction / destruction
    // ─────────────────────────────────────────────────────────────────────────

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

        SDL_Surface* surf = IMG_Load("res/breakout_spritesheet.png");
        if (!surf) { std::cout << SDL_GetError() << std::endl; return; }

        tex = SDL_CreateTextureFromSurface(ren, surf);
        SDL_DestroySurface(surf);
        if (!tex) { std::cout << SDL_GetError() << std::endl; return; }

        SDL_srand(time(nullptr));
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
        if (b2World_IsValid(box))
            b2DestroyWorld(box);
        if (tex)
            SDL_DestroyTexture(tex);
        if (ren)
            SDL_DestroyRenderer(ren);
        if (win)
            SDL_DestroyWindow(win);

        SDL_Quit();
    }

    bool Breakout::valid() const
    {
        return initialized && b2World_IsValid(box);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Main loop
    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::run()
    {
        auto  start = SDL_GetTicks();
        bool  quit  = false;

        while (!quit) {
            // Update all systems in order
            input_system();
            move_system();
            attached_ball_system();
            box_system();
            collision_system();   // writes gameOver / won into GameState

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            draw_system();
            SDL_RenderPresent(ren);

            // Frame-rate cap (same pattern as Pong)
            const auto end = SDL_GetTicks();
            if (end - start < GAME_FRAME)
                SDL_Delay(GAME_FRAME - (end - start));
            start += GAME_FRAME;

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                   (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE))
                    quit = true;
            }

            // End-game check: read GameState via mask — no entity search
            static const Mask stateMask = MaskBuilder()
                .set<StateTag>()
                .set<GameState>()
                .build();

            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (e.test(stateMask)) {
                    const auto& gs = e.get<GameState>();
                    if (gs.gameOver || gs.won) {
                        SDL_Delay(1500);
                        quit = true;
                    }
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Entity creation
    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::create_entities()
    {
        create_state();
        create_walls();
        create_bricks();
        create_paddle();
        create_ball();
        create_hearts();
    }

    void Breakout::create_state()
    {
        Entity::create().addAll(
            StateTag{},
            GameState{ 0, 3, BRICK_ROWS * BRICK_COLS, false, false }
        );
    }

    void Breakout::create_walls()
    {
        const float hw = static_cast<float>(WIN_W);
        const float hh = static_cast<float>(WIN_H);

        // Top wall
        Entity::create().addAll(
            Transform{{ hw / 2.0f, 15.0f }, 0.0f},
            make_drawable(WALL_SPRITE, { hw, 30.0f }),
            Collider{ create_body(hw / 2.0f, 15.0f, hw, 30.0f, b2_staticBody),
                      { hw / 2.0f, 15.0f }, true, true },
            WallTag{}
        );

        // Left wall
        Entity::create().addAll(
            Transform{{ 15.0f, hh / 2.0f }, 0.0f},
            make_drawable(WALL_SPRITE, { 30.0f, hh }),
            Collider{ create_body(15.0f, hh / 2.0f, 30.0f, hh, b2_staticBody),
                      { 15.0f, hh / 2.0f }, true, true },
            WallTag{}
        );

        // Right wall
        Entity::create().addAll(
            Transform{{ hw - 15.0f, hh / 2.0f }, 0.0f},
            make_drawable(WALL_SPRITE, { 30.0f, hh }),
            Collider{ create_body(hw - 15.0f, hh / 2.0f, 30.0f, hh, b2_staticBody),
                      { 15.0f, hh / 2.0f }, true, true },
            WallTag{}
        );

        // Bottom death-zone sensor — detects when the ball exits below the paddle
        Entity::create().addAll(
            Collider{ create_sensor(hw / 2.0f, hh + 20.0f, hw, 40.0f),
                      { hw / 2.0f, 20.0f }, false, true },
            DeathZoneTag{}
        );
    }

    void Breakout::create_bricks()
    {
        for (int row = 0; row < BRICK_ROWS; ++row) {
            for (int col = 0; col < BRICK_COLS; ++col) {
                const float x = BRICK_START_X + col * (BRICK_W + BRICK_GAP) + BRICK_W / 2.0f;
                const float y = BRICK_START_Y + row * (BRICK_H + BRICK_GAP) + BRICK_H / 2.0f;

                Entity::create().addAll(
                    Transform{{ x, y }, 0.0f},
                    make_drawable(brick_sprite(row), { BRICK_W, BRICK_H }),
                    Collider{ create_sensor(x, y, BRICK_W, BRICK_H),
                              { BRICK_W / 2.0f, BRICK_H / 2.0f }, false, true },
                    BrickTag{},
                    BrickData{ (BRICK_ROWS - row) * 10 }
                );
            }
        }
    }


    void Breakout::create_hearts()
    {
        static constexpr float HEART_W    = 24.0f;
        static constexpr float HEART_H    = 21.0f;
        static constexpr float HEART_Y    = 582.0f;
        static constexpr float HEART_STEP = 30.0f;

        const float startX = static_cast<float>(WIN_W) / 2.0f - HEART_STEP;
        for (int i = 0; i < 3; ++i) {
            const float x = startX + i * HEART_STEP;
            Entity::create().addAll(
                Transform{{ x, HEART_Y }, 0.0f},
                make_drawable(HEART_SPRITE, { HEART_W, HEART_H }),
                HeartTag{}
            );
        }
    }
    void Breakout::create_paddle()
    {
        Entity::create().addAll(
            Transform{{ static_cast<float>(WIN_W) / 2.0f, PADDLE_Y }, 0.0f},
            make_drawable(PADDLE_SPRITE, { PADDLE_W, PADDLE_H }),
            Collider{ create_body(static_cast<float>(WIN_W) / 2.0f, PADDLE_Y,
                                  PADDLE_W, PADDLE_H, b2_kinematicBody),
                      { PADDLE_W / 2.0f, PADDLE_H / 2.0f }, true, true },
            Intent{},
            Keys{ SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                  SDL_SCANCODE_SPACE, SDL_SCANCODE_R },
            PaddleTag{}
        );
    }

    void Breakout::create_ball()
    {
        const float ballY = PADDLE_Y - PADDLE_H / 2.0f - BALL_SIZE;

        b2BodyId body = create_body(static_cast<float>(WIN_W) / 2.0f, ballY,
                                    BALL_SIZE, BALL_SIZE, b2_dynamicBody, true);

        // Use reset_ball to set initial position and velocity
        reset_ball(body);

        Entity::create().addAll(
            Transform{{ static_cast<float>(WIN_W) / 2.0f, ballY }, 0.0f},
            make_drawable(BALL_SPRITE, { BALL_SIZE, BALL_SIZE }),
            Collider{ body, { BALL_SIZE / 2.0f, BALL_SIZE / 2.0f }, true, true },
            BallTag{},
            BallState{ true, 0 }
        );
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Box2D body helpers
    // ─────────────────────────────────────────────────────────────────────────

    b2BodyId Breakout::create_body(float x, float y, float w, float h,
                                   b2BodyType type, bool enableSensorEvents) const
    {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type     = type;
        bd.position = { x / BOX_SCALE, y / BOX_SCALE };
        b2BodyId body = b2CreateBody(box, &bd);

        b2ShapeDef sd = b2DefaultShapeDef();
        sd.enableSensorEvents   = enableSensorEvents;
        sd.density              = 1.0f;
        sd.material.friction    = 0.0f;
        sd.material.restitution = 1.0f;

        b2Polygon poly = b2MakeBox(w / BOX_SCALE / 2.0f, h / BOX_SCALE / 2.0f);
        b2CreatePolygonShape(body, &sd, &poly);

        return body;
    }

    b2BodyId Breakout::create_sensor(float x, float y, float w, float h) const
    {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type     = b2_staticBody;
        bd.position = { x / BOX_SCALE, y / BOX_SCALE };
        b2BodyId body = b2CreateBody(box, &bd);

        b2ShapeDef sd = b2DefaultShapeDef();
        sd.isSensor           = true;
        sd.enableSensorEvents = true;

        b2Polygon poly = b2MakeBox(w / BOX_SCALE / 2.0f, h / BOX_SCALE / 2.0f);
        b2CreatePolygonShape(body, &sd, &poly);

        return body;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Round-reset helpers
    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::reset_ball(b2BodyId ballBody) const
    {
        // Choose a random horizontal direction that is not too vertical
        float rx = SDL_randf() * 2.0f - 1.0f;
        if (SDL_fabsf(rx) < 0.25f) rx = rx < 0.0f ? -0.25f : 0.25f;

        b2Body_SetTransform(ballBody,
            { static_cast<float>(WIN_W) / 2.0f / BOX_SCALE,
              (PADDLE_Y - PADDLE_H / 2.0f - BALL_SIZE) / BOX_SCALE },
            b2MakeRot(0.0f));

        b2Body_SetLinearVelocity(ballBody,
            { rx * BALL_SPEED_X / BOX_SCALE,
              BALL_SPEED_Y      / BOX_SCALE });
    }

    void Breakout::reset_paddle() const
    {
        // Iterate paddle entities by mask
        static const Mask mask = MaskBuilder()
            .set<Collider>()
            .set<PaddleTag>()
            .build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(mask)) {
                const auto& c = e.get<Collider>();
                b2Body_SetTransform(c.body,
                    { static_cast<float>(WIN_W) / 2.0f / BOX_SCALE,
                      PADDLE_Y / BOX_SCALE },
                    b2MakeRot(0.0f));
                b2Body_SetLinearVelocity(c.body, {0.0f, 0.0f});
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Systems
    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::input_system() const
    {
        // Process every entity that has both keyboard bindings and an intention slot
        static const Mask mask = MaskBuilder()
            .set<Keys>()
            .set<Intent>()
            .build();

        SDL_PumpEvents();
        const bool* kb = SDL_GetKeyboardState(nullptr);

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(mask)) {
                const auto& k = e.get<Keys>();
                auto&       i = e.get<Intent>();

                i.moveX   = 0.0f;
                if (kb[k.left])
                    i.moveX -= 1.0f;
                if (kb[k.right])
                    i.moveX += 1.0f;
                i.launch  = kb[k.launch];
                i.restart = kb[k.restart];
            }
        }
    }

    void Breakout::move_system() const
    {
        // Move every entity that has a physics body, a transform, and player intent
        static const Mask mask = MaskBuilder()
            .set<Transform>()
            .set<Collider>()
            .set<Intent>()
            .set<PaddleTag>()
            .build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(mask)) {
                const auto& intent   = e.get<Intent>();
                const auto& collider = e.get<Collider>();

                // Apply horizontal velocity from intent
                b2Body_SetLinearVelocity(collider.body,
                    { intent.moveX * PADDLE_SPEED / BOX_SCALE, 0.0f });

                // Read back position from Box2D and clamp to playfield boundaries
                b2Vec2 pos = b2Body_GetPosition(collider.body);
                float  x   = pos.x * BOX_SCALE;

                const float minX = 30.0f + PADDLE_W / 2.0f;
                const float maxX = WIN_W  - 30.0f - PADDLE_W / 2.0f;

                if (x < minX) {
                    x = minX; b2Body_SetLinearVelocity(collider.body, {0.0f, 0.0f});
                }
                if (x > maxX) {
                    x = maxX; b2Body_SetLinearVelocity(collider.body, {0.0f, 0.0f});
                }

                b2Body_SetTransform(collider.body,
                    { x / BOX_SCALE, PADDLE_Y / BOX_SCALE }, b2MakeRot(0.0f));
                e.get<Transform>().p = { x, PADDLE_Y };
            }
        }
    }

    void Breakout::attached_ball_system() const
    {
        // Masks for the two relevant entity types
        static const Mask ballMask = MaskBuilder()
            .set<BallState>().set<BallTag>().set<Collider>().set<Transform>()
            .build();
        static const Mask paddleMask = MaskBuilder()
            .set<PaddleTag>().set<Intent>().set<Transform>()
            .build();

        // Read paddle position and launch intent from the paddle entity
        float paddleX = static_cast<float>(WIN_W) / 2.0f;
        bool  launch  = false;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(paddleMask)) continue;
            paddleX = e.get<Transform>().p.x;
            launch  = e.get<Intent>().launch;
        }

        // Update ball position while it is still attached to the paddle
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(ballMask)) continue;

            auto& bs = e.get<BallState>();
            if (!bs.attached) continue;

            const float ballY = PADDLE_Y - PADDLE_H / 2.0f - BALL_SIZE;
            e.get<Transform>().p = { paddleX, ballY };

            const auto& c = e.get<Collider>();
            b2Body_SetTransform(c.body,
                { paddleX / BOX_SCALE, ballY / BOX_SCALE }, b2MakeRot(0.0f));

            // Release the ball when the player presses launch — use reset_ball for velocity
            if (launch) {
                bs.attached = false;
                reset_ball(c.body);
            }
        }
    }

    void Breakout::box_system() const
    {
        // Advance physics and copy body positions back into Transform components
        static const Mask mask = MaskBuilder()
            .set<Transform>()
            .set<Collider>()
            .build();

        b2World_Step(box, BOX_STEP, 4);

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(mask)) {
                const auto& c = e.get<Collider>();

                if (b2Body_IsValid(c.body)) {
                    const auto tr = b2Body_GetTransform(c.body);
                    e.get<Transform>().p = { tr.p.x * BOX_SCALE,
                                             tr.p.y * BOX_SCALE };
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  collision_system helpers (private)
    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::on_death_zone(Entity ballEnt) const
    {
        static const Mask stateMask = MaskBuilder()
            .set<StateTag>().set<GameState>().build();

        // Decrement lives via GameState mask loop
        for (Entity s = Entity::first(); !s.eof(); s.next()) {
            if (s.test(stateMask)) {
                auto& gs = s.get<GameState>();
                --gs.lives;
                if (gs.lives <= 0)
                    gs.gameOver = true;
            }
        }

        // Remove one heart entity to reflect the lost life
        static const Mask heartMask = MaskBuilder()
            .set<HeartTag>().build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(heartMask)) {
                e.destroy();
                break;
            }
        }

        // Reposition ball and paddle for the next life
        reset_ball(ballEnt.get<Collider>().body);
        reset_paddle();
    }

    void Breakout::on_brick_hit(Entity ballEnt, Entity brickEnt) const
    {
        static const Mask stateMask = MaskBuilder()
            .set<StateTag>().set<GameState>().build();

        const auto& ballCollider = ballEnt.get<Collider>();

        // Update score and brick count via GameState mask loop
        for (Entity s = Entity::first(); !s.eof(); s.next()) {
            if (s.test(stateMask)) {
                auto& gs = s.get<GameState>();
                gs.score += brickEnt.get<BrickData>().value;
                --gs.bricksLeft;
                if (gs.bricksLeft <= 0) {
                    gs.won = true;
                    b2Body_SetLinearVelocity(ballCollider.body, {0.0f, 0.0f});
                }
            }
        }

        // Redirect ball horizontally based on offset from the brick centre
        const float ballX  = ballEnt.get<Transform>().p.x;
        const float brickX = brickEnt.get<Transform>().p.x;
        float norm = (ballX - brickX) / (BRICK_W / 2.0f);
        if (norm < -1.0f) norm = -1.0f;
        if (norm >  1.0f) norm =  1.0f;

        b2Vec2 vel = b2Body_GetLinearVelocity(ballCollider.body);
        vel.x = norm * BALL_SPEED_X / BOX_SCALE;
        vel.y = SDL_fabsf(vel.y);  // always bounce upward after hitting a brick
        b2Body_SetLinearVelocity(ballCollider.body, vel);

        // Destroy brick physics body and remove its ECS entity
        const auto& bc = brickEnt.get<Collider>();
        if (b2Body_IsValid(bc.body)) b2DestroyBody(bc.body);
        brickEnt.destroy();
    }

    void Breakout::on_paddle_contact(Entity ballEnt) const
    {
        static const Mask paddleMask = MaskBuilder()
            .set<PaddleTag>().set<Collider>().set<Transform>()
            .build();

        const auto& ballCollider = ballEnt.get<Collider>();

        constexpr int MAX_CONTACTS = 16;
        b2ContactData contacts[MAX_CONTACTS];
        const int cap   = b2Body_GetContactCapacity(ballCollider.body);
        const int count = cap > 0
            ? b2Body_GetContactData(ballCollider.body, contacts,
                                    cap < MAX_CONTACTS ? cap : MAX_CONTACTS)
            : 0;

        for (int i = 0; i < count; ++i) {
            if (contacts[i].manifold.pointCount == 0) continue;

            // Identify which of the two shapes belongs to the paddle
            const b2BodyId bodyA   = b2Shape_GetBody(contacts[i].shapeIdA);
            const b2BodyId bodyB   = b2Shape_GetBody(contacts[i].shapeIdB);
            const b2BodyId bid     = ballCollider.body;
            const b2BodyId otherBid =
                (bodyA.index1 == bid.index1 &&
                 bodyA.world0 == bid.world0 &&
                 bodyA.generation == bid.generation) ? bodyB : bodyA;

            // Find the paddle entity that owns this body via mask loop
            for (Entity paddle = Entity::first(); !paddle.eof(); paddle.next()) {
                if (!paddle.test(paddleMask)) continue;

                const b2BodyId pb = paddle.get<Collider>().body;
                if (pb.index1 != otherBid.index1 ||
                    pb.world0 != otherBid.world0  ||
                    pb.generation != otherBid.generation)
                    continue;

                // Deflect ball: horizontal direction depends on where it hit the paddle
                const float norm =
                    (ballEnt.get<Transform>().p.x -
                     paddle.get<Transform>().p.x) / (PADDLE_W / 2.0f);

                b2Vec2 vel = b2Body_GetLinearVelocity(ballCollider.body);
                vel.x = norm * BALL_SPEED_X / BOX_SCALE;
                vel.y = -SDL_fabsf(vel.y);  // always bounce upward after paddle hit
                b2Body_SetLinearVelocity(ballCollider.body, vel);
                break;
            }
        }

        // Enforce minimum ball speed so the game never stalls
        b2Vec2 vel        = b2Body_GetLinearVelocity(ballCollider.body);
        const float speed = SDL_sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (speed > 0.001f && speed < BALL_MIN_SPEED) {
            const float f = BALL_MIN_SPEED / speed;
            b2Body_SetLinearVelocity(ballCollider.body, { vel.x * f, vel.y * f });
        }
    }

    // ─────────────────────────────────────────────────────────────────────────

    void Breakout::collision_system() const
    {
        // ── Sensor events (bricks and death zone) ─────────────────────────────
        const b2SensorEvents se = b2World_GetSensorEvents(box);

        for (int i = 0; i < se.beginCount; ++i) {
            const auto& ev = se.beginEvents[i];

            if (!b2Shape_IsValid(ev.sensorShapeId) ||
                !b2Shape_IsValid(ev.visitorShapeId))
                continue;

            const b2BodyId sensorBody  = b2Shape_GetBody(ev.sensorShapeId);
            const b2BodyId visitorBody = b2Shape_GetBody(ev.visitorShapeId);

            // Match sensor and visitor bodies to ECS entities via Collider mask
            static const Mask colMask = MaskBuilder().set<Collider>().build();

            Entity sensorEnt {{ World::maxId() + 1 }};
            Entity ballEnt   {{ World::maxId() + 1 }};

            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(colMask)) continue;
                const auto& c = e.get<Collider>();
                if (!b2Body_IsValid(c.body)) continue;

                const b2BodyId bid = c.body;
                if (bid.index1 == sensorBody.index1 &&
                    bid.world0 == sensorBody.world0  &&
                    bid.generation == sensorBody.generation)
                    sensorEnt = e;

                if (bid.index1 == visitorBody.index1 &&
                    bid.world0 == visitorBody.world0  &&
                    bid.generation == visitorBody.generation)
                    ballEnt = e;
            }

            if (sensorEnt.eof() || ballEnt.eof() || !ballEnt.has<BallTag>())
                continue;

            if (sensorEnt.has<DeathZoneTag>()) {
                on_death_zone(ballEnt);
                return;
            }

            if (sensorEnt.has<BrickTag>()) {
                on_brick_hit(ballEnt, sensorEnt);
                break;
            }
        }

        // ── Paddle contact: redirect the ball ─────────────────────────────────
        static const Mask ballMask = MaskBuilder()
            .set<BallTag>().set<Collider>().set<Transform>().set<BallState>()
            .build();

        for (Entity ball = Entity::first(); !ball.eof(); ball.next()) {
            if (!ball.test(ballMask)) continue;
            if (ball.get<BallState>().attached) continue;
            on_paddle_contact(ball);
        }
    }

    void Breakout::draw_system() const
    {
        // Render every entity that has a position and a sprite
        static const Mask mask = MaskBuilder()
            .set<Transform>()
            .set<Drawable>()
            .build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(mask)) {
                const auto& t = e.get<Transform>();
                const auto& d = e.get<Drawable>();

                SDL_FRect dest {
                    t.p.x - d.size.x / 2.0f,
                    t.p.y - d.size.y / 2.0f,
                    d.size.x, d.size.y
                };
                SDL_RenderTextureRotated(ren, tex, &d.part, &dest,
                                         t.angle, nullptr, SDL_FLIP_NONE);
            }
        }

        // Overlay end-game message — read from GameState via mask, no entity search
        static const Mask stateMask = MaskBuilder()
            .set<StateTag>().set<GameState>().build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(stateMask)) {
                const auto& gs = e.get<GameState>();
                if (!gs.won && !gs.gameOver) continue;

                SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
                SDL_FRect banner { 220.0f, 250.0f, 360.0f, 90.0f };
                SDL_RenderFillRect(ren, &banner);
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);

                if (gs.won)
                    SDL_RenderDebugText(ren, 350.0f, 290.0f, "YOU WIN");
                else
                    SDL_RenderDebugText(ren, 340.0f, 290.0f, "GAME OVER");
            }
        }
    }

} // namespace Game