# Breakout ECS – Assignment 3

## Authors
- May Hajbi
- Yuval Sigavi

## Description
Implementation of the Breakout game using the Entity Component System (ECS) architecture,
built on top of the Bagel ECS engine (bagel.h) developed in class.

## Libraries
- Bagel (bagel.h) — ECS engine
- SDL3 — window, rendering, input
- SDL3_image — spritesheet loading
- Box2D — physics simulation and collision detection

## Architecture
The game follows the ECS pattern as taught in class:
- **Components** — Transform, Drawable, Collider, Intent, Keys, BallState, BrickData, GameState, and tag components
- **Systems** — input_system, move_system, attached_ball_system, box_system, collision_system, draw_system
- Input is separated from movement via the Intent component

## Implemented
- ECS structure (Entities, Components, Systems)
- Paddle movement via Box2D kinematic body
- Ball physics via Box2D dynamic body
- Sensor-based collision detection (bricks, death zone)
- Brick destruction with score tracking
- Lives system with heart icons
- Win / lose conditions with end-game overlay

## How to generate documentation
```bash
doxygen doxyfile
```
Open `html/index.html` in a browser to view the generated documentation.