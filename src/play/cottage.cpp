#include "play/cottage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cauldron::play {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDropSeconds = 0.65f;
constexpr float kStirSeconds = 1.5f;
constexpr V3 kPotBase{0.0f, 1.2f, 0.3f};  // the cauldron's centre, resting on its fire

float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
Rgb mix(Rgb a, Rgb b, float t) { return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t)}; }
Rgb scale(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }

// A little hash: the same "random" layout every time (the polka dots stay put).
float hash01(uint32_t n) {
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return static_cast<float>(n & 0xffffffu) / static_cast<float>(0x1000000);
}

uint32_t gSeed = 12345u;
float rnd() {
    gSeed = gSeed * 1664525u + 1013904223u;
    return static_cast<float>(gSeed >> 8) / static_cast<float>(1u << 24);
}
float rnd(float lo, float hi) { return lo + (hi - lo) * rnd(); }

Rgb hue(float h) {
    h = h - std::floor(h);
    const float r = clampf(std::fabs(h * 6 - 3) - 1, 0, 1);
    const float g = clampf(2 - std::fabs(h * 6 - 2), 0, 1);
    const float b = clampf(2 - std::fabs(h * 6 - 4), 0, 1);
    return {r, g, b};
}

struct Draw {
    std::vector<StrataRenderItem> items;
    std::vector<StrataPoint> points;

    void prim(int kind, V3 p, V3 s, Rgb c) {
        StrataRenderItem it{};
        it.kind = kind;
        it.position[0] = p.x;
        it.position[1] = p.y;
        it.position[2] = p.z;
        it.scale[0] = s.x;
        it.scale[1] = s.y;
        it.scale[2] = s.z;
        it.color[0] = c.r;
        it.color[1] = c.g;
        it.color[2] = c.b;
        it.color[3] = 1;
        items.push_back(it);
    }
    void cube(V3 p, V3 s, Rgb c) { prim(STRATA_PRIMITIVE_CUBE, p, s, c); }
    void sphere(V3 p, V3 s, Rgb c) { prim(STRATA_PRIMITIVE_SPHERE, p, s, c); }
    void cyl(V3 p, V3 s, Rgb c) { prim(STRATA_PRIMITIVE_CYLINDER, p, s, c); }
    void dot(V3 p, float worldSize, Rgb c, float alpha, bool soft, float minPixels = 1.5f) {
        StrataPoint pt{};
        pt.position[0] = p.x;
        pt.position[1] = p.y;
        pt.position[2] = p.z;
        pt.size_pixels = minPixels;
        pt.world_diameter = worldSize;
        pt.color[0] = c.r;
        pt.color[1] = c.g;
        pt.color[2] = c.b;
        pt.color[3] = alpha;
        pt.flags = soft ? STRATA_POINT_SOFT : 0u;
        points.push_back(pt);
    }
};

const V3 kLanterns[4] = {{-4.4f, 0.36f, 1.2f}, {4.5f, 0.36f, 0.9f}, {-2.9f, 0.3f, 2.9f}, {3.1f, 0.3f, 3.1f}};

// One friendly bat: a body and two flapping wings.
void bat(Draw& d, V3 at, float flap, Rgb body, float size) {
    d.dot(at, 0.26f * size, body, 1.0f, false, 3);
    const float w = 0.22f * size;
    const float up = std::sin(flap) * 0.12f * size;
    d.dot({at.x - w, at.y + up, at.z}, 0.2f * size, body, 0.95f, false, 2);
    d.dot({at.x + w, at.y + up, at.z}, 0.2f * size, body, 0.95f, false, 2);
    d.dot({at.x - w * 1.7f, at.y + up * 1.8f, at.z}, 0.13f * size, body, 0.9f, false, 2);
    d.dot({at.x + w * 1.7f, at.y + up * 1.8f, at.z}, 0.13f * size, body, 0.9f, false, 2);
    d.dot({at.x - 0.05f * size, at.y + 0.03f * size, at.z + 0.1f}, 0.05f * size, {1, 0.9f, 0.4f}, 1, false, 1);
    d.dot({at.x + 0.05f * size, at.y + 0.03f * size, at.z + 0.1f}, 0.05f * size, {1, 0.9f, 0.4f}, 1, false, 1);
}

// A frog, sitting at `at` (its feet), facing the room. With a crown if it is proud.
void frog(Draw& d, V3 at, float squash, bool crown) {
    const Rgb green{0.22f, 0.72f, 0.18f}, belly{0.75f, 0.9f, 0.45f};
    const float h = 0.45f * squash;
    d.sphere({at.x, at.y + h * 0.5f, at.z}, {0.7f, h, 0.6f}, green);
    d.sphere({at.x, at.y + h * 0.42f, at.z + 0.14f}, {0.5f, h * 0.7f, 0.38f}, belly);
    for (int s = -1; s <= 1; s += 2) {
        const float ex = at.x + s * 0.18f, ey = at.y + h + 0.06f;
        d.sphere({ex, ey, at.z + 0.1f}, {0.22f, 0.22f, 0.22f}, green);
        d.sphere({ex, ey + 0.02f, at.z + 0.18f}, {0.14f, 0.14f, 0.1f}, {0.95f, 0.95f, 0.9f});
        d.dot({ex, ey + 0.02f, at.z + 0.25f}, 0.06f, {0.02f, 0.02f, 0.02f}, 1, false, 2);
        d.sphere({at.x + s * 0.3f, at.y + 0.06f, at.z + 0.2f}, {0.26f, 0.1f, 0.3f}, green);  // feet
    }
    d.dot({at.x, at.y + h * 0.55f, at.z + 0.32f}, 0.06f, {0.6f, 0.15f, 0.15f}, 1, false, 2);  // a smile, nearly
    if (crown) {
        const Rgb gold{1.0f, 0.78f, 0.15f};
        const float cy = at.y + h + 0.24f;
        d.cyl({at.x, cy, at.z + 0.05f}, {0.32f, 0.07f, 0.32f}, gold);
        for (int k = -1; k <= 1; ++k) d.cube({at.x + k * 0.11f, cy + 0.12f, at.z + 0.12f}, {0.07f, 0.14f, 0.07f}, gold);
        d.dot({at.x, cy + 0.06f, at.z + 0.22f}, 0.07f, {0.9f, 0.1f, 0.3f}, 1, false, 2);
    }
}

}  // namespace

Cottage::Cottage(strata::Engine& engine, std::function<void(const char*)> cue) : engine_(engine), cue_(std::move(cue)) {
    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SPACE);
    light.legacy_ambient = 0.22f;
    engine_.setLighting(light);
}

V3 Cottage::jarPos(int i) const {
    const float xs[4] = {-2.7f, -0.9f, 0.9f, 2.7f};
    const float y = i < 4 ? 2.5f + 0.34f : 3.7f + 0.34f;
    return {xs[i % 4], y, -3.5f};
}

bool Cottage::add(Ingredient i) {
    if (stirring() || !game_.add(i)) {
        if (!game_.stirred && static_cast<int>(game_.pot.size()) >= kCapacity) cue_("full");
        return false;
    }
    Drop d;
    d.what = i;
    d.from = jarPos(static_cast<int>(i));
    drops_.push_back(d);
    cue_("pick");
    return true;
}

bool Cottage::stir() {
    if (stirring() || game_.stirred || game_.pot.empty() || game_.done()) return false;
    stirT_ = 0;
    cue_("stir");
    return true;
}

void Cottage::judge() {
    // Any spoonful still in the air lands now.
    landed_ = static_cast<int>(game_.pot.size());
    drops_.clear();
    target_ = brewColor(game_.pot);
    game_.stir();
    effectOn_ = true;
    effectT_ = 0;
    beat_ = 0;
    const Verdict& v = game_.last;
    if (v.outcome == Outcome::Success) {
        target_ = game_.done() ? Rgb{} : recipes()[game_.recipe].color;
        cue_("success");
        spawnSplash({1, 1, 1}, 40, 3.2f);
        return;
    }
    switch (v.outcome) {
    case Outcome::Frog: cue_("ribbit"); break;
    case Outcome::Disco: cue_("disco"); break;
    case Outcome::Floaty: cue_("floaty"); break;
    case Outcome::BubbleBats: cue_("bats"); break;
    case Outcome::PolkaDots: cue_("dots"); break;
    case Outcome::Hiccups: cue_("hic"); break;
    case Outcome::Sprinkles: cue_("sprinkles"); break;
    case Outcome::BigBubble: cue_("grow"); break;
    case Outcome::Fizzle: cue_("fizzle"); target_ = {0.28f, 0.28f, 0.27f}; break;
    case Outcome::Burp: cue_("burp"); break;
    default: break;
    }
}

bool Cottage::pourOut() {
    if (stirring() || game_.done()) return false;
    if (game_.stirred && game_.last.outcome == Outcome::Success) return false;
    if (game_.pot.empty() && !game_.stirred) return false;
    game_.pourOut();
    drops_.clear();
    landed_ = 0;
    effectOn_ = false;
    target_ = brewColor({});
    pourT_ = 0;
    cue_("pour");
    spawnPuff({kPotBase.x, kPotBase.y + 1.1f, kPotBase.z}, {0.75f, 0.75f, 0.8f}, 10, 0.5f);
    return true;
}

bool Cottage::next() {
    if (!game_.stirred || game_.last.outcome != Outcome::Success) return false;
    game_.next();
    effectOn_ = false;
    landed_ = 0;
    target_ = brewColor({});
    cue_(game_.done() ? "finale" : "next");
    if (game_.done()) celebrateT_ = 0;
    return true;
}

void Cottage::restart() {
    game_ = Game{};
    drops_.clear();
    parts_.clear();
    landed_ = 0;
    stirT_ = -1;
    effectOn_ = false;
    celebrateT_ = -1;
    target_ = brewColor({});
    cue_("next");
}

void Cottage::peek() { game_.peeked = game_.canPeek() || game_.peeked; }

std::string Cottage::effectName() const {
    if (celebrateT_ >= 0) return "finale";
    if (!effectOn_) return "";
    switch (game_.last.outcome) {
    case Outcome::Success: return "success";
    case Outcome::Frog: return "frog";
    case Outcome::Disco: return "disco";
    case Outcome::Floaty: return "floaty";
    case Outcome::BubbleBats: return "bubble-bats";
    case Outcome::PolkaDots: return "polka-dots";
    case Outcome::Hiccups: return "hiccups";
    case Outcome::Sprinkles: return "sprinkles";
    case Outcome::BigBubble: return "big-bubble";
    case Outcome::Fizzle: return "fizzle";
    case Outcome::Burp: return "burp";
    }
    return "";
}

bool Cottage::project(V3 p, float& nx, float& ny) const {
    const float* v = cam_.view;
    const float* pr = cam_.projection;
    float e[4];
    const float in[4] = {p.x, p.y, p.z, 1};
    for (int r = 0; r < 4; ++r) e[r] = v[0 * 4 + r] * in[0] + v[1 * 4 + r] * in[1] + v[2 * 4 + r] * in[2] + v[3 * 4 + r] * in[3];
    float c[4];
    for (int r = 0; r < 4; ++r) c[r] = pr[0 * 4 + r] * e[0] + pr[1 * 4 + r] * e[1] + pr[2 * 4 + r] * e[2] + pr[3 * 4 + r] * e[3];
    if (c[3] <= 1e-4f) return false;
    nx = (c[0] / c[3]) * 0.5f + 0.5f;
    ny = 1.0f - ((c[1] / c[3]) * 0.5f + 0.5f);
    return true;
}

bool Cottage::jarOnScreen(Ingredient i, float& nx, float& ny) const {
    return project(jarPos(static_cast<int>(i)), nx, ny) && nx >= 0 && nx <= 1 && ny >= 0 && ny <= 1;
}

int Cottage::tap(float nx, float ny) {
    // The nearest jar within reach of the tap, measured on screen in pixels
    // (a jar is about a finger wide at phone sizes, so the reach is generous).
    int best = -1;
    float bestD = 1e9f;
    for (int i = 0; i < kIngredientCount; ++i) {
        float jx, jy;
        if (!project(jarPos(i), jx, jy)) continue;
        const float dx = (jx - nx) * static_cast<float>(width_), dy = (jy - ny) * static_cast<float>(height_);
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    float ax, ay, bx, by;
    project(jarPos(0), ax, ay);
    project(jarPos(1), bx, by);
    const float spacing = std::fabs(bx - ax) * static_cast<float>(width_);
    if (best < 0 || bestD > std::max(24.0f, spacing * 0.55f)) return -1;
    add(static_cast<Ingredient>(best));
    return best;
}

void Cottage::spawnSplash(Rgb c, int n, float speed) {
    const V3 top{kPotBase.x, kPotBase.y + potLift_ + 0.95f, kPotBase.z};
    for (int k = 0; k < n; ++k) {
        Particle p;
        const float a = rnd(0, 2 * kPi), r = rnd(0.1f, 0.6f);
        p.p = {top.x + std::cos(a) * r, top.y, top.z + std::sin(a) * r};
        p.v = {std::cos(a) * rnd(0.3f, 1.0f), rnd(0.6f, 1.0f) * speed, std::sin(a) * rnd(0.3f, 1.0f)};
        p.life = rnd(0.6f, 1.2f);
        p.size = rnd(0.07f, 0.16f);
        p.color = mix(c, {1, 1, 1}, rnd(0, 0.4f));
        p.soft = false;
        p.gravity = 6.0f;
        parts_.push_back(p);
    }
}

void Cottage::spawnPuff(V3 at, Rgb c, int n, float size) {
    for (int k = 0; k < n; ++k) {
        Particle p;
        p.p = {at.x + rnd(-0.3f, 0.3f), at.y + rnd(-0.1f, 0.2f), at.z + rnd(-0.3f, 0.3f)};
        p.v = {rnd(-0.4f, 0.4f), rnd(0.5f, 1.2f), rnd(-0.2f, 0.4f)};
        p.life = rnd(1.4f, 2.4f);
        p.size = size * rnd(0.7f, 1.3f);
        p.grow = size * 0.8f;
        p.color = c;
        p.alpha = 0.45f;
        parts_.push_back(p);
    }
}

void Cottage::updateParticles(float dt) {
    for (Particle& p : parts_) {
        p.age += dt;
        p.v.y -= p.gravity * dt;
        p.p.x += p.v.x * dt;
        p.p.y += p.v.y * dt;
        p.p.z += p.v.z * dt;
        p.size += p.grow * dt;
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.age >= p.life; }), parts_.end());
    if (parts_.size() > 900) parts_.erase(parts_.begin(), parts_.begin() + static_cast<long>(parts_.size() - 900));
}

void Cottage::frame(double dtSeconds) {
    const float dt = clampf(static_cast<float>(dtSeconds), 0.0f, 0.25f);
    time_ += dt;
    int32_t w = 1, h = 1;
    engine_.drawableSize(w, h);
    width_ = std::max(1, static_cast<int>(w));
    height_ = std::max(1, static_cast<int>(h));

    // Spoonfuls in flight land in the pot.
    for (Drop& d : drops_) {
        d.t += dt / kDropSeconds;
        if (d.t >= 1) {
            ++landed_;
            std::vector<Ingredient> so(game_.pot.begin(), game_.pot.begin() + std::min<size_t>(landed_, game_.pot.size()));
            target_ = brewColor(so);
            spawnSplash(info(d.what).color, 14, 2.2f);
            spawnPuff({kPotBase.x, kPotBase.y + potLift_ + 1.2f, kPotBase.z}, mix(info(d.what).color, {0.8f, 0.8f, 0.8f}, 0.5f), 3, 0.35f);
            cue_("plop");
        }
    }
    drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [](const Drop& d) { return d.t >= 1; }), drops_.end());

    if (stirT_ >= 0) {
        stirT_ += dt;
        if (stirT_ >= kStirSeconds) {
            stirT_ = -1;
            judge();
        }
    }
    if (pourT_ >= 0) {
        pourT_ += dt;
        if (pourT_ > 1.0f) pourT_ = -1;
    }
    if (celebrateT_ >= 0) celebrateT_ += dt;

    // The colour eases towards what is in the pot.
    const float k = 1.0f - std::exp(-dt * 3.0f);
    brew_ = mix(brew_, target_, k);

    // The cauldron's height: floating, hiccuping, or at rest.
    float lift = 0;
    const Outcome out = game_.last.outcome;
    if (effectOn_) {
        effectT_ += dt;
        const float before = beat_;
        beat_ += dt;
        if (out == Outcome::Floaty) lift = std::min(effectT_ * 0.9f, 1.3f) + std::sin(effectT_ * 2.2f) * 0.15f;
        if (out == Outcome::Hiccups) {
            const float period = 1.1f;
            if (std::floor(before / period) != std::floor(beat_ / period)) {
                potJump_ = 0.35f;
                cue_("hic");
                spawnPuff({kPotBase.x, kPotBase.y + 1.3f, kPotBase.z}, {0.35f, 0.33f, 0.36f}, 9, 0.55f);
            }
        }
        if (out == Outcome::Burp) {
            const float period = 2.6f;
            if (std::floor(before / period) != std::floor(beat_ / period)) {
                potJump_ = 0.25f;
                cue_("burp");
            }
        }
        if (out == Outcome::Frog) {
            // A ribbit at each landing of a hop (hops every 1/1.2 s once out of the pot).
            const float hop = 1.0f / 1.2f;
            if (effectT_ > 0.9f && std::floor((before - 0.9f) / hop) != std::floor((beat_ - 0.9f) / hop)) cue_("ribbit");
        }
        if (out == Outcome::BigBubble) {
            const float period = 4.0f;
            const float a = std::fmod(before, period), b = std::fmod(beat_, period);
            if (a < 2.8f && b >= 2.8f) {
                cue_("boing");
                spawnSplash({0.95f, 0.95f, 1.0f}, 30, 3.0f);
            }
            if (b < a) cue_("grow");
        }
        if (out == Outcome::Fizzle) {
            const float period = 2.2f;
            if (std::floor(before / period) != std::floor(beat_ / period)) {
                spawnPuff({kPotBase.x, kPotBase.y + 1.1f, kPotBase.z}, {0.5f, 0.5f, 0.5f}, 3, 0.25f);
                cue_("pfft");
            }
        }
        if (out == Outcome::Success && !game_.done()) {
            // Sparkles keep coming for a while.
            if (effectT_ < 6.0f && rnd() < dt * 30) {
                const Rgb c = recipes()[static_cast<size_t>(game_.recipe)].color;
                spawnSplash(mix(c, {1, 1, 1}, 0.3f), 2, 3.4f);
            }
        }
    }
    if (celebrateT_ >= 0 && rnd() < dt * 40) spawnSplash(hue(time_ * 0.2f + rnd()), 2, 3.8f);
    potJump_ = std::max(0.0f, potJump_ - dt * 1.2f);
    potLift_ = lift + std::sin(std::min(potJump_, 0.35f) / 0.35f * kPi) * potJump_;

    // Bubbles and steam, more of both the fuller the pot and while stirring.
    const float fill = static_cast<float>(landed_);
    const V3 top{kPotBase.x, kPotBase.y + potLift_ + 0.96f, kPotBase.z};
    const bool fizzled = effectOn_ && out == Outcome::Fizzle;
    bubbleClock_ += dt * (fizzled ? 1.5f : 4.0f + fill * 2.0f + (stirring() ? 18.0f : 0.0f));
    while (bubbleClock_ >= 1) {
        bubbleClock_ -= 1;
        Particle p;
        const float a = rnd(0, 2 * kPi), r = std::sqrt(rnd()) * 0.8f;
        p.p = {top.x + std::cos(a) * r, top.y, top.z + std::sin(a) * r};
        p.v = {0, rnd(0.15f, 0.4f), 0};
        p.life = rnd(0.25f, 0.6f);
        p.size = rnd(0.07f, 0.17f);
        p.grow = 0.1f;
        p.color = mix(brew_, {1, 1, 1}, 0.35f);
        p.alpha = 0.9f;
        p.soft = false;
        parts_.push_back(p);
    }
    steamClock_ += dt * (2.0f + fill * 0.5f);
    while (steamClock_ >= 1) {
        steamClock_ -= 1;
        Particle p;
        p.p = {top.x + rnd(-0.5f, 0.5f), top.y + 0.2f, top.z + rnd(-0.5f, 0.5f)};
        p.v = {rnd(-0.1f, 0.1f), rnd(0.5f, 0.8f), rnd(-0.05f, 0.1f)};
        p.life = rnd(2.0f, 3.2f);
        p.size = rnd(0.3f, 0.5f);
        p.grow = 0.5f;
        p.color = mix(brew_, {0.85f, 0.85f, 0.85f}, 0.55f);
        p.alpha = 0.16f;
        parts_.push_back(p);
    }
    updateParticles(dt);

    const float ease = 1.0f - std::exp(-dt * 6.0f);
    coverTop_ += (wantTop_ - coverTop_) * ease;
    coverBottom_ += (wantBottom_ - coverBottom_) * ease;
    camera();
    lights();
    draw();
}

void Cottage::setCover(float top, float bottom) {
    wantTop_ = clampf(top, 0.0f, 0.6f);
    wantBottom_ = clampf(bottom, 0.0f, 0.6f);
    if (wantTop_ + wantBottom_ > 0.7f) {
        const float k = 0.7f / (wantTop_ + wantBottom_);
        wantTop_ *= k;
        wantBottom_ *= k;
    }
}

void Cottage::camera() {
    // The cottage is framed in the part of the canvas the page leaves clear:
    // a virtual viewport `open` of the height, then squeezed into place.
    const float open = 1.0f - coverTop_ - coverBottom_;
    const float aspect = static_cast<float>(width_) / (static_cast<float>(height_) * open);
    const V3 target{0.0f, 2.25f, -1.0f};
    V3 eye{0.0f, 4.5f, 6.9f};
    const V3 dir{eye.x - target.x, eye.y - target.y, eye.z - target.z};
    const float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    // Keep the shelves (4.3 either side) and the floor to the jars in view at
    // any shape of screen: widen the lens first, then step back.
    const float halfW = 4.3f, halfH = 2.7f;
    float tanHalf = std::max({std::tan(46.0f * 0.5f * kPi / 180.0f), halfW / (dist * aspect), halfH / dist});
    const float maxTan = std::tan(72.0f * 0.5f * kPi / 180.0f);
    float d = dist;
    if (tanHalf > maxTan) {
        d = std::max(halfW / (maxTan * aspect), halfH / maxTan);
        tanHalf = maxTan;
    }
    const float k = d / dist;
    eye = {target.x + dir.x * k, target.y + dir.y * k, target.z + dir.z * k};
    const float fovY = 2.0f * std::atan(tanHalf) * 180.0f / kPi;  // degrees
    const float e[3] = {eye.x, eye.y, eye.z}, t[3] = {target.x, target.y, target.z}, up[3] = {0, 1, 0};
    cam_ = strata::cameraLookAt(e, t, up, fovY, aspect, 0.1f, 60.0f);
    // NDC y = s * y' + o: [-1, 1] of the virtual view lands between the covers.
    const float sY = open, oY = coverBottom_ - coverTop_;
    for (int col = 0; col < 4; ++col) cam_.projection[col * 4 + 1] = sY * cam_.projection[col * 4 + 1] + oY * cam_.projection[col * 4 + 3];
    engine_.setCamera(cam_);
}

void Cottage::lights() {
    std::vector<StrataPointLight> ls;
    auto add = [&](V3 p, float range, Rgb c, float intensity) {
        StrataPointLight l{};
        l.position[0] = p.x;
        l.position[1] = p.y;
        l.position[2] = p.z;
        l.range = range;
        l.color[0] = clampf(c.r, 0, 1);
        l.color[1] = clampf(c.g, 0, 1);
        l.color[2] = clampf(c.b, 0, 1);
        l.intensity = intensity;
        ls.push_back(l);
    };
    const bool disco = effectOn_ && game_.last.outcome == Outcome::Disco;
    const bool fizzled = effectOn_ && game_.last.outcome == Outcome::Fizzle;
    const float flicker = 1.0f + 0.12f * std::sin(time_ * 13.0f) + 0.08f * std::sin(time_ * 7.3f + 1.0f);
    add({0.0f, 0.5f, 1.2f}, 9.0f, disco ? hue(time_ * 0.7f) : Rgb{1.0f, 0.55f, 0.2f}, (fizzled ? 0.8f : 1.5f) * flicker);
    add({0.0f, kPotBase.y + potLift_ + 1.5f, kPotBase.z + 0.4f}, 4.5f, mix(brew_, {1, 1, 1}, 0.2f), 0.6f);
    add({0.0f, 4.8f, -1.6f}, 7.0f, disco ? hue(time_ * 0.7f + 0.5f) : Rgb{1.0f, 0.82f, 0.6f}, 0.55f);
    add({0.0f, 5.0f, 5.5f}, 12.0f, {0.75f, 0.72f, 0.85f}, 0.35f);
    add({-3.55f, 3.2f, -3.0f}, 3.5f, {1.0f, 0.75f, 0.4f}, 0.7f * flicker);
    add({3.55f, 3.2f, -3.0f}, 3.5f, {1.0f, 0.75f, 0.4f}, 0.7f * flicker);
    add({-4.6f, 4.4f, -3.2f}, 3.5f, {0.5f, 0.6f, 1.0f}, 0.35f);
    if (disco)
        for (int i = 0; i < 4; ++i) {
            const float a = time_ * 1.3f + static_cast<float>(i) * kPi * 0.5f;
            add({std::cos(a) * 3.5f, 3.0f + std::sin(a * 1.7f), -1.0f + std::sin(a) * 2.5f}, 5.0f, hue(time_ * 0.5f + i * 0.25f), 1.0f);
        }
    const bool lanterns = celebrateT_ >= 0 ||
                          (effectOn_ && game_.last.outcome == Outcome::Success && recipes()[std::min(game_.recipe, recipeCount() - 1)].flourish == Flourish::Lanterns);
    for (int i = 0; i < 4; ++i)
        if (i < 2 || lanterns) add({kLanterns[i].x, kLanterns[i].y + 0.3f, kLanterns[i].z + 0.5f}, 2.5f, {1.0f, 0.6f, 0.15f}, 0.6f * flicker);
    engine_.setPointLights(ls.data(), ls.size());
}

void Cottage::draw() {
    Draw d;
    const Outcome out = game_.last.outcome;
    const bool success = effectOn_ && out == Outcome::Success;
    const Flourish flourish = recipes()[std::min(game_.recipe, recipeCount() - 1)].flourish;
    const bool celebrate = celebrateT_ >= 0;

    // ---- the room
    const Rgb wood{0.34f, 0.21f, 0.12f}, darkWood{0.22f, 0.13f, 0.07f}, stone{0.34f, 0.30f, 0.32f};
    for (int i = 0; i < 9; ++i)
        d.cube({-8.0f + 2.0f * i, -0.1f, 1.0f}, {1.96f, 0.2f, 14.0f}, i % 2 ? wood : scale(wood, 0.86f));
    d.cube({0.0f, 3.5f, -4.15f}, {16.0f, 7.0f, 0.2f}, scale(stone, 0.8f));
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 9; ++col) {
            const float off = row % 2 ? 0.0f : 0.8f;
            const float shade = 0.85f + 0.25f * hash01(static_cast<uint32_t>(row * 31 + col * 7));
            d.cube({-6.8f + col * 1.6f + off, 0.45f + row * 0.85f, -4.02f}, {1.5f, 0.78f, 0.1f}, scale(stone, shade));
        }
    d.cube({-6.6f, 3.5f, 0.0f}, {0.2f, 7.0f, 14.0f}, scale(stone, 0.7f));
    d.cube({6.6f, 3.5f, 0.0f}, {0.2f, 7.0f, 14.0f}, scale(stone, 0.7f));
    d.cube({0.0f, 6.6f, -3.6f}, {16.0f, 0.35f, 0.4f}, darkWood);  // a beam

    // The window, with the night and the moon.
    d.cube({-4.75f, 4.6f, -3.95f}, {1.6f, 1.6f, 0.06f}, darkWood);
    d.cube({-4.75f, 4.6f, -3.9f}, {1.36f, 1.36f, 0.04f}, {0.04f, 0.06f, 0.2f});
    d.cube({-4.75f, 4.6f, -3.86f}, {0.08f, 1.36f, 0.04f}, darkWood);
    d.cube({-4.75f, 4.6f, -3.86f}, {1.36f, 0.08f, 0.04f}, darkWood);
    d.dot({-4.45f, 4.9f, -3.84f}, 0.36f, {1.0f, 0.97f, 0.85f}, 1.0f, false, 2);
    d.dot({-4.45f, 4.9f, -3.83f}, 1.0f, {0.8f, 0.85f, 1.0f}, 0.25f, true, 2);
    for (int s = 0; s < 6; ++s) {
        const float tw = 0.5f + 0.5f * std::sin(time_ * 2.0f + s * 1.7f);
        d.dot({-5.3f + 0.22f * s, 4.1f + 0.9f * hash01(static_cast<uint32_t>(s + 3)), -3.85f}, 0.05f, {1, 1, 0.9f}, 0.5f + 0.5f * tw, false, 1.5f);
    }

    // The shelves of jars.
    for (float y : {2.44f, 3.64f}) {
        d.cube({0.0f, y, -3.55f}, {7.8f, 0.12f, 0.8f}, wood);
        for (float x : {-3.6f, 0.0f, 3.6f}) d.cube({x, y - 0.22f, -3.85f}, {0.14f, 0.34f, 0.2f}, darkWood);
    }
    for (int i = 0; i < kIngredientCount; ++i) {
        const V3 p = jarPos(i);
        const Rgb c = info(static_cast<Ingredient>(i)).color;
        // A jar wiggles as its spoonful leaves.
        float lift = 0;
        for (const Drop& dr : drops_)
            if (static_cast<int>(dr.what) == i && dr.t < 0.4f) lift = std::sin(dr.t / 0.4f * kPi) * 0.12f;
        const V3 q{p.x, p.y + lift, p.z};
        d.cyl(q, {0.62f, 0.34f, 0.62f}, mix(c, {0.9f, 0.9f, 0.95f}, 0.15f));
        d.cyl({q.x, q.y + 0.4f, q.z}, {0.36f, 0.07f, 0.36f}, {0.62f, 0.44f, 0.24f});
        d.cube({q.x, q.y - 0.02f, q.z + 0.31f}, {0.36f, 0.24f, 0.02f}, {0.93f, 0.86f, 0.68f});
        d.dot({q.x, q.y - 0.02f, q.z + 0.34f}, 0.12f, scale(c, 0.85f), 1.0f, false, 2);
        d.dot(q, 1.0f, c, 0.16f + 0.05f * std::sin(time_ * 2.0f + i), true, 2);
    }
    // Candles at the ends of the lower shelf.
    for (float x : {-3.55f, 3.55f}) {
        d.cyl({x, 2.75f, -3.45f}, {0.2f, 0.19f, 0.2f}, {0.95f, 0.9f, 0.78f});
        const float f = 0.9f + 0.1f * std::sin(time_ * 11.0f + x);
        d.dot({x, 3.04f, -3.45f}, 0.13f * f, {1.0f, 0.85f, 0.4f}, 1.0f, true, 3);
        d.dot({x, 3.04f, -3.45f}, 0.6f, {1.0f, 0.7f, 0.3f}, 0.2f, true, 2);
    }
    // A black cat asleep at the end of the top shelf; its eyes open now and then.
    {
        const V3 c{3.62f, 3.92f, -3.45f};
        const Rgb black{0.05f, 0.05f, 0.07f};
        const float breathe = 1.0f + 0.04f * std::sin(time_ * 1.6f);
        d.sphere(c, {0.62f, 0.36f * breathe, 0.42f}, black);
        d.sphere({c.x - 0.3f, c.y + 0.12f, c.z + 0.08f}, {0.32f, 0.3f, 0.3f}, black);
        d.cube({c.x - 0.39f, c.y + 0.3f, c.z + 0.08f}, {0.08f, 0.12f, 0.06f}, black);
        d.cube({c.x - 0.21f, c.y + 0.3f, c.z + 0.08f}, {0.08f, 0.12f, 0.06f}, black);
        d.cyl({c.x + 0.25f, c.y - 0.12f, c.z + 0.22f}, {0.1f, 0.05f, 0.4f}, black);
        const bool awake = std::fmod(time_, 9.0f) > 7.0f || (effectOn_ && out != Outcome::Success && effectT_ < 2.5f);
        if (awake)
            for (float s : {-0.07f, 0.07f}) d.dot({c.x - 0.3f + s, c.y + 0.15f, c.z + 0.25f}, 0.06f, {0.85f, 0.95f, 0.2f}, 1, false, 2);
    }

    // The bottled potions, on the low shelves either side of the hearth.
    d.cube({-2.75f, 1.3f, -3.6f}, {2.5f, 0.1f, 0.6f}, wood);
    d.cube({2.75f, 1.3f, -3.6f}, {2.5f, 0.1f, 0.6f}, wood);
    const int bottled = static_cast<int>(game_.triesPerRecipe.size());
    for (int k = 0; k < recipeCount(); ++k) {
        const float x = k < 4 ? -3.75f + k * 0.62f : 1.85f + (k - 4) * 0.62f;
        const V3 p{x, 1.55f, -3.55f};
        if (k < bottled) {
            const Rgb c = recipes()[static_cast<size_t>(k)].color;
            d.sphere(p, {0.36f, 0.36f, 0.36f}, c);
            d.cyl({p.x, p.y + 0.26f, p.z}, {0.13f, 0.1f, 0.13f}, mix(c, {1, 1, 1}, 0.4f));
            d.cyl({p.x, p.y + 0.38f, p.z}, {0.11f, 0.04f, 0.11f}, {0.62f, 0.44f, 0.24f});
            d.dot(p, 0.6f, c, 0.3f, true, 2);
        } else {
            d.dot(p, 0.18f, {0.5f, 0.45f, 0.4f}, 0.25f, false, 2);  // a dusty ring where a bottle will go
        }
    }

    // Jack-o'-lanterns.
    const bool allLit = celebrate || (success && flourish == Flourish::Lanterns);
    for (int i = 0; i < 4; ++i) {
        const V3 p = kLanterns[i];
        const bool lit = i < 2 || (allLit && (celebrate || effectT_ > 0.3f * i));
        d.sphere(p, {0.85f, 0.66f, 0.8f}, {0.95f, 0.42f, 0.05f});
        d.cyl({p.x, p.y + 0.36f, p.z}, {0.1f, 0.08f, 0.1f}, {0.3f, 0.35f, 0.1f});
        const Rgb face = lit ? Rgb{1.0f, 0.85f, 0.3f} : Rgb{0.12f, 0.05f, 0.02f};
        const float fz = p.z + 0.41f;
        d.dot({p.x - 0.15f, p.y + 0.08f, fz}, 0.11f, face, 1, false, 2);
        d.dot({p.x + 0.15f, p.y + 0.08f, fz}, 0.11f, face, 1, false, 2);
        for (int m = -2; m <= 2; ++m) d.dot({p.x + m * 0.07f, p.y - 0.1f - (m == 0 ? 0.02f : 0.0f), fz}, 0.07f, face, 1, false, 2);
        if (lit) d.dot({p.x, p.y, fz}, 1.0f, {1.0f, 0.6f, 0.2f}, 0.22f, true, 2);
    }

    // ---- the hearth and the cauldron
    d.cube({0.0f, 0.1f, kPotBase.z}, {1.7f, 0.2f, 0.26f}, darkWood);
    d.cube({0.0f, 0.1f, kPotBase.z}, {0.26f, 0.2f, 1.7f}, scale(darkWood, 1.2f));
    for (int k = 0; k < 10; ++k) {
        const float a = k * 0.63f;
        const V3 sp{std::cos(a) * 1.05f, 0.1f, kPotBase.z + std::sin(a) * 1.05f};
        d.sphere(sp, {0.34f, 0.24f, 0.3f}, scale(stone, 0.9f + 0.2f * hash01(static_cast<uint32_t>(k))));
    }
    const bool fizzled = effectOn_ && out == Outcome::Fizzle;
    for (int k = 0; k < 8; ++k) {
        const float ph = time_ * (2.4f + k * 0.3f) + k * 1.3f;
        const float s = (fizzled ? 0.6f : 1.0f) * (0.42f + 0.16f * std::sin(ph));
        const V3 fp{std::sin(k * 2.1f) * 0.42f, 0.32f + 0.12f * std::sin(ph * 0.7f) + (k % 3) * 0.08f, kPotBase.z + 0.35f + std::cos(k * 1.7f) * 0.25f};
        const Rgb fc = mix({1.0f, 0.35f, 0.05f}, {1.0f, 0.85f, 0.3f}, 0.5f + 0.5f * std::sin(ph * 1.3f));
        d.dot(fp, s, fc, 0.75f, true, 3);
    }
    d.dot({0.0f, 0.35f, kPotBase.z + 0.6f}, 2.0f, {1.0f, 0.5f, 0.15f}, fizzled ? 0.08f : 0.16f, true, 2);

    const V3 c{kPotBase.x, kPotBase.y + potLift_, kPotBase.z};
    const Rgb iron{0.09f, 0.09f, 0.11f};
    d.sphere(c, {2.4f, 1.8f, 2.4f}, iron);
    d.cyl({c.x, c.y + 0.8f, c.z}, {2.0f, 0.12f, 2.0f}, iron);
    for (int k = 0; k < 22; ++k) {
        const float a = k * 2.0f * kPi / 22.0f;
        d.sphere({c.x + std::cos(a) * 0.98f, c.y + 0.95f, c.z + std::sin(a) * 0.98f}, {0.24f, 0.2f, 0.24f}, scale(iron, 1.4f));
    }
    for (int k = 0; k < 3; ++k) {
        const float a = kPi * 0.5f + k * 2.0f * kPi / 3.0f;
        d.cube({c.x + std::cos(a) * 0.75f, c.y - 0.85f, c.z + std::sin(a) * 0.75f}, {0.22f, 0.5f, 0.22f}, iron);
    }
    // The brew: its surface, its glow, and a swirl while it is stirred.
    Rgb surface = brew_;
    if (effectOn_ && out == Outcome::Disco) surface = hue(time_ * 0.8f);
    if (celebrate) surface = hue(time_ * 0.3f);
    const float pourDip = pourT_ >= 0 ? std::sin(pourT_ * kPi) * 0.3f : 0.0f;
    d.cyl({c.x, c.y + 0.93f, c.z}, {1.78f, 0.02f, 1.78f}, mix(surface, {0, 0, 0}, pourDip));
    d.dot({c.x, c.y + 1.05f, c.z + 0.1f}, 1.7f, surface, 0.32f, true, 2);
    if (stirring()) {
        const float a = stirT_ * 2.0f * kPi * 1.4f;
        const V3 sp{c.x + std::cos(a) * 0.45f, c.y + 1.75f, c.z + std::sin(a) * 0.45f};
        d.cyl(sp, {0.12f, 0.85f, 0.12f}, {0.55f, 0.36f, 0.18f});
        d.sphere({sp.x, sp.y + 0.9f, sp.z}, {0.2f, 0.2f, 0.2f}, {0.5f, 0.32f, 0.16f});
        for (int k = 0; k < 14; ++k) {
            const float b = a - k * 0.35f;
            const float r = 0.25f + 0.04f * k;
            d.dot({c.x + std::cos(b) * r, c.y + 0.98f, c.z + std::sin(b) * r}, 0.12f, mix(surface, {1, 1, 1}, 0.5f), 0.8f - k * 0.05f, false, 2);
        }
    }

    // Spoonfuls on their way from the shelf to the pot.
    for (const Drop& dr : drops_) {
        const float t = clampf(dr.t, 0, 1);
        const V3 end{c.x, c.y + 1.0f, c.z};
        const V3 mid{(dr.from.x + end.x) * 0.5f, std::max(dr.from.y, end.y) + 1.4f, (dr.from.z + end.z) * 0.5f};
        const float u = 1 - t;
        const V3 p{u * u * dr.from.x + 2 * u * t * mid.x + t * t * end.x, u * u * dr.from.y + 2 * u * t * mid.y + t * t * end.y,
                   u * u * dr.from.z + 2 * u * t * mid.z + t * t * end.z};
        const Rgb col = info(dr.what).color;
        d.sphere(p, {0.3f, 0.3f, 0.3f}, col);
        d.dot(p, 0.6f, col, 0.35f, true, 3);
    }

    // ---- what the brew did
    if (effectOn_) {
        const float t = effectT_;
        switch (out) {
        case Outcome::Frog: {
            V3 at;
            float squash = 1;
            if (t < 0.9f) {  // out of the pot in one leap
                const float u = t / 0.9f;
                at = {lerp(c.x, 1.6f, u), c.y + 1.0f + std::sin(u * kPi) * 1.5f - u * (c.y + 1.0f), lerp(c.z, 2.4f, u)};
            } else {
                const float h = (t - 0.9f) * 1.2f;
                const float ph = h - std::floor(h);
                const float x = 1.6f * std::cos((t - 0.9f) * 0.75f);
                at = {x, std::sin(ph * kPi) * 0.6f, 2.4f + 0.3f * std::sin((t - 0.9f) * 0.75f)};
                squash = ph < 0.12f ? 0.75f : 1.0f;
            }
            frog(d, at, squash, false);
            break;
        }
        case Outcome::Disco:
            for (int k = 0; k < 40; ++k) {
                const float a = k * 0.9f + time_ * 0.6f;
                const V3 p{std::cos(a) * (2.0f + (k % 5) * 0.8f), 1.0f + (k % 7) * 0.8f, -3.9f + (k % 3) * 0.01f};
                d.dot(p, 0.45f, hue(time_ * 0.5f + k * 0.13f), 0.55f, true, 3);
            }
            for (int k = 0; k < 12; ++k) {
                const float a = time_ * 1.5f + k * 0.52f;
                d.dot({std::cos(a) * 1.6f, 0.02f, c.z + 1.4f + std::sin(a) * 1.0f}, 0.5f, hue(k / 12.0f + time_), 0.5f, true, 3);
            }
            break;
        case Outcome::Floaty:
            for (int k = 0; k < 3; ++k) d.dot({c.x + (k - 1) * 0.6f, c.y - 1.2f - k * 0.1f, c.z + 0.6f}, 0.25f, {0.75f, 0.85f, 1.0f}, 0.4f, true, 2);
            break;
        case Outcome::BubbleBats:
            for (int k = 0; k < 9; ++k) {
                const float a = t * 0.9f + k * 0.7f;
                const V3 p{std::sin(a) * 4.0f, 3.0f + std::sin(a * 2.0f) * 1.2f + (k % 3) * 0.3f, -0.8f + std::cos(a) * 1.8f};
                bat(d, p, t * 14.0f + k, {0.72f, 0.55f, 0.95f}, 1.1f);
                d.dot(p, 0.55f, {0.8f, 0.7f, 1.0f}, 0.2f, true, 2);
            }
            break;
        case Outcome::PolkaDots: {
            const Rgb dots[5] = {{1.0f, 0.5f, 0.1f}, {1.0f, 0.4f, 0.7f}, {1.0f, 0.9f, 0.2f}, {0.2f, 0.85f, 0.8f}, {0.6f, 0.4f, 1.0f}};
            const int shown = std::min(160, static_cast<int>(t * 90.0f));
            for (int k = 0; k < shown; ++k) {
                const float u = hash01(static_cast<uint32_t>(k * 3 + 1)), v = hash01(static_cast<uint32_t>(k * 3 + 2));
                const int face = k % 4;
                V3 p;
                if (face == 0 || face == 3) p = {-6.5f + u * 13.0f, 0.2f + v * 6.2f, -3.92f};  // the back wall
                else if (face == 1) p = {-6.45f, 0.2f + v * 6.2f, -4.0f + u * 9.0f};        // left wall
                else p = {6.45f, 0.2f + v * 6.2f, -4.0f + u * 9.0f};                       // right wall
                if (k % 9 == 4) p = {-6.0f + u * 12.0f, 0.02f, -3.0f + v * 7.0f};          // the floor
                d.dot(p, 0.42f, dots[k % 5], 1.0f, false, 3);
            }
            for (int k = 0; k < 10; ++k) {  // and the cauldron, too
                const float a = k * 0.63f;
                d.dot({c.x + std::cos(a) * 1.1f, c.y + std::sin(k * 1.3f) * 0.5f, c.z + std::fabs(std::sin(a)) * 1.15f + 0.05f}, 0.28f, dots[k % 5], 1.0f, false, 3);
            }
            break;
        }
        case Outcome::Sprinkles:
            for (int k = 0; k < 120; ++k) {
                const float u = hash01(static_cast<uint32_t>(k * 5 + 11)), v = hash01(static_cast<uint32_t>(k * 5 + 12));
                const float speed = 1.2f + hash01(static_cast<uint32_t>(k + 99)) * 1.0f;
                const float y = 6.4f - std::fmod(t * speed + v * 6.4f, 6.4f);
                const Rgb col = hue(hash01(static_cast<uint32_t>(k + 500)));
                d.dot({-5.5f + u * 11.0f, y, -3.0f + v * 6.0f}, 0.1f, mix(col, {1, 1, 1}, 0.2f), 1.0f, false, 2);
            }
            break;
        case Outcome::BigBubble: {
            const float ph = std::fmod(t, 4.0f);
            if (ph < 2.8f) {
                const float s = 0.4f + ph / 2.8f * 3.4f;
                const V3 p{c.x, c.y + 1.0f + s * 0.45f, c.z};
                d.dot(p, s, {0.85f, 0.9f, 1.0f}, 0.22f, true, 3);
                d.dot(p, s * 0.92f, hue(time_ * 0.2f), 0.08f, true, 3);
                d.dot({p.x - s * 0.22f, p.y + s * 0.22f, p.z + s * 0.3f}, s * 0.14f, {1, 1, 1}, 0.6f, true, 2);
            }
            break;
        }
        case Outcome::Burp: {
            const float ph = std::fmod(t, 2.6f);
            const float r = 0.4f + ph * 1.4f;
            for (int k = 0; k < 28; ++k) {
                const float a = k * 2.0f * kPi / 28.0f;
                d.dot({c.x + std::cos(a) * r, c.y + 1.3f + ph * 1.2f, c.z + std::sin(a) * r * 0.6f}, 0.5f, {0.5f, 0.6f, 0.4f},
                      0.4f * (1.0f - ph / 2.6f), true, 3);
            }
            break;
        }
        case Outcome::Success: {
            const Rgb pc = recipes()[std::min(game_.recipe, recipeCount() - 1)].color;
            if (flourish == Flourish::Moonrise) {
                const V3 p{c.x, c.y + 1.2f + std::min(t, 2.0f) * 1.1f + std::sin(t * 1.5f) * 0.1f, c.z};
                d.sphere(p, {0.6f, 0.6f, 0.6f}, {0.95f, 0.95f, 0.85f});
                d.dot(p, 1.8f, {0.85f, 0.9f, 1.0f}, 0.35f, true, 3);
                for (int k = 0; k < 8; ++k) {
                    const float a = t * 0.8f + k * 0.785f;
                    d.dot({p.x + std::cos(a) * 1.0f, p.y + std::sin(a * 2.0f) * 0.2f, p.z + std::sin(a) * 1.0f}, 0.1f, {1, 1, 0.8f}, 1, false, 2);
                }
            } else if (flourish == Flourish::FrogKing) {
                const float hop = std::fabs(std::sin(t * 3.0f)) * 0.25f;
                frog(d, {c.x, c.y + 0.98f + hop, c.z + 0.2f}, 1.0f, true);
            } else if (flourish == Flourish::Starfall) {
                for (int k = 0; k < 60; ++k) {
                    const float u = hash01(static_cast<uint32_t>(k * 7 + 3)), v = hash01(static_cast<uint32_t>(k * 7 + 4));
                    const float y = 6.2f - std::fmod(t * 0.6f + v * 6.2f, 6.2f);
                    d.dot({-5.0f + u * 10.0f + std::sin(t + k) * 0.2f, y, -3.0f + v * 5.5f}, 0.16f, {1.0f, 0.85f, 0.35f}, 1, false, 2);
                    d.dot({-5.0f + u * 10.0f + std::sin(t + k) * 0.2f, y, -3.0f + v * 5.5f}, 0.5f, {1.0f, 0.8f, 0.3f}, 0.15f, true, 2);
                }
            } else if (flourish == Flourish::Wings) {
                for (int k = 0; k < 7; ++k) {
                    const float a = t * 1.2f + k * 0.9f;
                    const V3 p{std::sin(a) * 3.2f, 3.6f + std::sin(a * 2.0f) * 0.8f, -0.5f + std::cos(a) * 1.6f};
                    bat(d, p, t * 16.0f + k, {0.12f, 0.08f, 0.16f}, 1.3f);
                }
            }
            d.dot({c.x, c.y + 1.6f, c.z}, 2.4f, pc, 0.25f + 0.1f * std::sin(t * 4.0f), true, 3);
            break;
        }
        default: break;
        }
    }
    if (celebrate) {
        for (int k = 0; k < 60; ++k) {
            const float u = hash01(static_cast<uint32_t>(k * 7 + 3)), v = hash01(static_cast<uint32_t>(k * 7 + 4));
            const float y = 6.2f - std::fmod(celebrateT_ * 0.6f + v * 6.2f, 6.2f);
            d.dot({-5.0f + u * 10.0f, y, -3.0f + v * 5.5f}, 0.16f, hue(u + celebrateT_ * 0.1f), 1, false, 2);
        }
        frog(d, {c.x + 1.9f, 0.0f + std::fabs(std::sin(celebrateT_ * 3.0f)) * 0.3f, c.z + 1.6f}, 1.0f, true);
        for (int k = 0; k < 5; ++k) {
            const float a = celebrateT_ * 1.1f + k * 1.25f;
            bat(d, {std::sin(a) * 3.4f, 4.2f + std::sin(a * 2.0f) * 0.6f, -0.6f + std::cos(a) * 1.4f}, celebrateT_ * 15.0f + k, {0.12f, 0.08f, 0.16f}, 1.2f);
        }
    }

    // Particles: splashes, bubbles, steam and puffs.
    for (const Particle& p : parts_) {
        const float life = p.age / p.life;
        const float a = p.alpha * (life < 0.8f ? 1.0f : (1.0f - life) / 0.2f);
        d.dot(p.p, p.size, p.color, a, p.soft, p.soft ? 2.0f : 1.5f);
    }

    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
    fd.clear_color[0] = 0.03f;
    fd.clear_color[1] = 0.02f;
    fd.clear_color[2] = 0.04f;
    fd.clear_color[3] = 1.0f;
    engine_.beginFrame(fd);
    engine_.submitPrimitives(d.items.data(), d.items.size());
    if (!d.points.empty()) engine_.submitPoints(d.points.data(), d.points.size());
    engine_.endFrame();
}

}  // namespace cauldron::play
