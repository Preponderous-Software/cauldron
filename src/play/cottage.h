// The witch's cottage on Strata: the shelves of jars, the cauldron over its
// fire, and everything a brew does. Owns the evening (sim/brew.h) and turns
// the player's taps into it, with the animations in between.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "sim/brew.h"
#include "strata/strata.hpp"

namespace cauldron::play {

struct V3 {
    float x = 0, y = 0, z = 0;
};

class Cottage {
public:
    // cue(name) is called for every sound the page should make ("plop", "ribbit", ...).
    Cottage(strata::Engine& engine, std::function<void(const char*)> cue);

    // Advances the animations by dt seconds and draws a frame.
    void frame(double dt);

    // The player's actions. Each returns false when it does nothing now.
    bool add(Ingredient i);
    bool stir();
    bool pourOut();
    bool next();
    void restart();
    void peek();

    // A tap at (nx, ny), 0..1 across and down the canvas: the ingredient
    // whose jar is there (it is added), or -1.
    int tap(float nx, float ny);
    // How much of the canvas the page covers at the top and bottom (0..1 of
    // its height): the view eases to fit the cottage into the rest.
    void setCover(float top, float bottom);

    const Game& game() const { return game_; }
    // True while the spoon is going round (the verdict comes after).
    bool stirring() const { return stirT_ >= 0; }
    // What the room is doing now: "", or the outcome / flourish in play.
    std::string effectName() const;
    // A jar's position on screen (0..1), for tests: x, y.
    bool jarOnScreen(Ingredient i, float& nx, float& ny) const;

private:
    struct Drop {
        Ingredient what;
        V3 from;
        float t = 0;
    };
    struct Particle {
        V3 p, v;
        float life = 0, age = 0, size = 0;
        Rgb color;
        float alpha = 1;
        bool soft = true;
        float grow = 0;
        float gravity = 0;
    };

    void judge();
    void spawnSplash(Rgb c, int n, float speed);
    void spawnPuff(V3 at, Rgb c, int n, float size);
    void updateParticles(float dt);
    void camera();
    void lights();
    void draw();
    V3 jarPos(int i) const;
    bool project(V3 p, float& nx, float& ny) const;

    strata::Engine& engine_;
    std::function<void(const char*)> cue_;
    Game game_;
    StrataCamera cam_{};
    int width_ = 1, height_ = 1;

    float time_ = 0;
    Rgb brew_{0.10f, 0.16f, 0.14f};   // shown colour, easing towards target_
    Rgb target_{0.10f, 0.16f, 0.14f};
    int landed_ = 0;                  // spoonfuls that have reached the pot
    std::vector<Drop> drops_;
    std::vector<Particle> parts_;
    float bubbleClock_ = 0, steamClock_ = 0;
    float stirT_ = -1;                // seconds into the stir, or -1
    bool effectOn_ = false;
    float effectT_ = 0;               // seconds since the verdict
    float beat_ = 0;                  // a repeating effect's own clock
    float potLift_ = 0;               // the cauldron's height above its fire (Floaty, Hiccups)
    float potJump_ = 0;
    float pourT_ = -1;                // the pour-out splash
    float celebrateT_ = -1;           // the finished book
    float coverTop_ = 0, coverBottom_ = 0, wantTop_ = 0, wantBottom_ = 0;
};

}  // namespace cauldron::play
