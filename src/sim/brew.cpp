#include "sim/brew.h"

#include <algorithm>

namespace cauldron {

namespace {

using I = Ingredient;

const IngredientInfo kInfo[kIngredientCount] = {
    {"Gummy Newt", "Gummy Newts", "wiggly, green, chewy", {0.20f, 0.75f, 0.12f}},
    {"Glow Mushroom", "Glow Mushrooms", "shines in the dark", {0.10f, 0.85f, 0.80f}},
    {"Moon Water", "Moon Water", "caught by moonlight", {0.55f, 0.65f, 0.95f}},
    {"Bat Wing Leaf", "Bat Wing Leaves", "a flappy purple leaf", {0.45f, 0.12f, 0.65f}},
    {"Pumpkin Seeds", "Pumpkin Seeds", "a handful, toasted", {1.00f, 0.45f, 0.05f}},
    {"Ember Pepper", "Ember Peppers", "warm as a hearth", {0.90f, 0.10f, 0.05f}},
    {"Star Sugar", "Star Sugar", "sparkly and sweet", {1.00f, 0.85f, 0.30f}},
    {"Spider Silk", "Spider Silk", "one soft thread", {0.92f, 0.92f, 0.95f}},
};

std::vector<Recipe> makeRecipes() {
    std::vector<Recipe> r;
    r.push_back({"Glow Juice", {I::GlowMushroom, I::MoonWater}, false, {}, {0.25f, 0.95f, 0.85f}, Flourish::Fountain,
                 "It glows like a nightlight in a jar."});
    r.push_back({"Pumpkin Fizz", {I::PumpkinSeeds, I::PumpkinSeeds, I::EmberPepper}, false, {}, {1.0f, 0.50f, 0.08f},
                 Flourish::Lanterns, "Every jack-o'-lantern in the room lights up at once."});
    r.push_back({"Moonbeam Draught", {I::MoonWater, I::MoonWater, I::StarSugar, I::GlowMushroom}, false, {},
                 {0.75f, 0.82f, 1.0f}, Flourish::Moonrise, "A tiny moon floats up out of the pot and hums."});
    r.push_back({"Hoppity Tonic", {I::MoonWater, I::GummyNewt, I::BatWing, I::PumpkinSeeds}, false, {},
                 {0.45f, 0.90f, 0.25f}, Flourish::FrogKing, "A very proud frog in a very small crown."});
    r.push_back({"Night Flight Elixir", {I::BatWing, I::BatWing, I::SpiderSilk, I::StarSugar, I::MoonWater}, false, {},
                 {0.55f, 0.30f, 0.95f}, Flourish::Wings, "A flock of friendly bats does a loop-the-loop."});
    r.push_back({"Sweet Dreams Syrup",
                 {I::MoonWater, I::StarSugar, I::StarSugar, I::SpiderSilk},
                 true,
                 {"Begin with a puddle the sky fell into.", "Then two spoons of something that twinkles and tastes sweet.",
                  "Finish with one thread a spider spun."},
                 {0.95f, 0.75f, 0.95f},
                 Flourish::Starfall,
                 "Golden stars drift down, slow as a lullaby."});
    r.push_back({"Bonfire Brew",
                 {I::EmberPepper, I::PumpkinSeeds, I::PumpkinSeeds, I::PumpkinSeeds, I::GlowMushroom},
                 true,
                 {"Something hot goes in first.", "Then the seeds of a jack-o'-lantern, once for each corner of a triangle.",
                  "Last, one light from the forest floor."},
                 {1.0f, 0.30f, 0.10f},
                 Flourish::Lanterns,
                 "Warm as a bonfire, and every lantern in the room winks."});
    r.push_back({"Grand Halloween Potion",
                 {I::GlowMushroom, I::GummyNewt, I::BatWing, I::BatWing, I::StarSugar},
                 true,
                 {"Begin the way you began your very first potion.", "Add the chewy green fellow who is almost a lizard.",
                  "Then the leaves that look like wings, twice.", "End with the sweetest thing on the shelf."},
                 {1.0f, 0.62f, 0.95f},
                 Flourish::Fountain,
                 "Happy Halloween! The whole cottage sparkles."});
    return r;
}

}  // namespace

const IngredientInfo& info(Ingredient i) { return kInfo[static_cast<int>(i)]; }

Outcome mischief(Ingredient i) {
    switch (i) {
    case I::GummyNewt: return Outcome::Frog;
    case I::GlowMushroom: return Outcome::Disco;
    case I::MoonWater: return Outcome::Floaty;
    case I::BatWing: return Outcome::BubbleBats;
    case I::PumpkinSeeds: return Outcome::PolkaDots;
    case I::EmberPepper: return Outcome::Hiccups;
    case I::StarSugar: return Outcome::Sprinkles;
    case I::SpiderSilk: return Outcome::BigBubble;
    }
    return Outcome::Fizzle;
}

const char* outcomeTitle(Outcome o) {
    switch (o) {
    case Outcome::Success: return "Perfect!";
    case Outcome::Frog: return "Ribbit! A frog!";
    case Outcome::Disco: return "Disco cottage!";
    case Outcome::Floaty: return "The cauldron floated away!";
    case Outcome::BubbleBats: return "Bubble bats!";
    case Outcome::PolkaDots: return "Polka dots everywhere!";
    case Outcome::Hiccups: return "Hic! Hic! Hiccups!";
    case Outcome::Sprinkles: return "It's raining sprinkles!";
    case Outcome::BigBubble: return "Boing! A giant bubble!";
    case Outcome::Fizzle: return "Pfffft. A fizzle.";
    case Outcome::Burp: return "BURP!";
    }
    return "";
}

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> r = makeRecipes();
    return r;
}

int recipeCount() { return static_cast<int>(recipes().size()); }

std::vector<std::string> cardLines(const Recipe& r) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < r.steps.size()) {
        size_t j = i;
        while (j < r.steps.size() && r.steps[j] == r.steps[i]) ++j;
        const int n = static_cast<int>(j - i);
        const IngredientInfo& in = info(r.steps[i]);
        std::string line = out.empty() ? "" : "then ";
        line += std::to_string(n) + " " + (n == 1 ? in.name : in.plural);
        out.push_back(line);
        i = j;
    }
    return out;
}

Verdict judge(const Recipe& r, const std::vector<Ingredient>& added) {
    Verdict v;
    if (added == r.steps) {
        v.outcome = Outcome::Success;
        v.message = std::string("You made ") + r.name + "! " + r.result;
        return v;
    }
    std::array<int, kIngredientCount> have{}, need{};
    for (Ingredient i : added) ++have[static_cast<int>(i)];
    for (Ingredient i : r.steps) ++need[static_cast<int>(i)];
    for (Ingredient i : added) {
        const int k = static_cast<int>(i);
        if (have[k] > need[k]) {
            v.outcome = mischief(i);
            v.hasCulprit = true;
            v.culprit = i;
            if (need[k] == 0)
                v.message = std::string(info(i).name) + " doesn't belong in " + r.name + ".";
            else
                v.message = std::string("A spoonful too much ") + info(i).name + ".";
            v.message += " Pour it out and try again!";
            return v;
        }
    }
    for (Ingredient i : r.steps) {
        const int k = static_cast<int>(i);
        if (have[k] < need[k]) {
            v.outcome = Outcome::Fizzle;
            v.hasCulprit = true;
            v.culprit = i;
            if (r.riddle)
                v.message = "Something is missing. Read the clues again!";
            else
                v.message = std::string("It needed more ") + info(i).name + ". Pour it out and try again!";
            return v;
        }
    }
    v.outcome = Outcome::Burp;
    v.message = "The right things, in a muddled order. Pour it out and follow the card from the top!";
    return v;
}

Rgb brewColor(const std::vector<Ingredient>& added) {
    Rgb c{0.10f, 0.16f, 0.14f};
    int n = 0;
    for (Ingredient i : added) {
        ++n;
        const Rgb k = info(i).color;
        // Early spoonfuls change it most; later ones blend in.
        const float w = n == 1 ? 0.75f : 0.45f;
        c.r += (k.r - c.r) * w;
        c.g += (k.g - c.g) * w;
        c.b += (k.b - c.b) * w;
    }
    return c;
}

int matchedPrefix(const Recipe& r, const std::vector<Ingredient>& added) {
    int n = 0;
    while (n < static_cast<int>(added.size()) && n < static_cast<int>(r.steps.size()) && added[n] == r.steps[n]) ++n;
    return n;
}

bool Game::add(Ingredient i) {
    if (done() || stirred || static_cast<int>(pot.size()) >= kCapacity) return false;
    pot.push_back(i);
    return true;
}

bool Game::stir() {
    if (done() || stirred || pot.empty()) return false;
    last = judge(recipes()[recipe], pot);
    stirred = true;
    if (last.outcome != Outcome::Success) ++tries;
    return true;
}

void Game::pourOut() {
    if (stirred && last.outcome == Outcome::Success) return;  // a finished potion is bottled, not poured
    pot.clear();
    stirred = false;
    last = Verdict{};
}

void Game::next() {
    if (!stirred || last.outcome != Outcome::Success) return;
    triesPerRecipe.push_back(tries);
    ++recipe;
    tries = 0;
    pot.clear();
    stirred = false;
    peeked = false;
    last = Verdict{};
}

bool Game::canPeek() const { return !done() && recipes()[recipe].riddle && tries >= 2; }

int Game::stars(int t) { return t == 0 ? 3 : t <= 2 ? 2 : 1; }

int Game::totalStars() const {
    int s = 0;
    for (int t : triesPerRecipe) s += stars(t);
    return s;
}

}  // namespace cauldron
