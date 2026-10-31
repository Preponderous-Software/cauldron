// The rules: recipes, judging a stir, the brew's colour, the evening's flow.
#include <set>
#include <string>

#include "check.h"
#include "sim/brew.h"

using namespace cauldron;
using I = Ingredient;

static void recipesAreSound() {
    CHECK(recipeCount() >= 6);
    std::set<std::string> names;
    bool riddleSeen = false;
    for (const Recipe& r : recipes()) {
        CHECK(!r.steps.empty());
        CHECK(static_cast<int>(r.steps.size()) <= kCapacity);
        CHECK(names.insert(r.name).second);  // no two share a name
        // Riddles come last, and carry clues; cards carry none.
        if (r.riddle) riddleSeen = true;
        CHECK(r.riddle == riddleSeen);
        CHECK(r.riddle ? r.clues.size() >= 2 : r.clues.empty());
        CHECK(judge(r, r.steps).outcome == Outcome::Success);
        CHECK(r.result && *r.result);
    }
    CHECK(riddleSeen);
    // Recipes get longer, never shorter, from the first to the last.
    CHECK(recipes().front().steps.size() <= recipes().back().steps.size());
    // The last riddle begins the way the first recipe began.
    CHECK(recipes().back().steps.front() == recipes().front().steps.front());
}

static void cardsReadNaturally() {
    const Recipe& fizz = recipes()[1];
    const auto lines = cardLines(fizz);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "2 Pumpkin Seeds");
    CHECK(lines[1] == "then 1 Ember Pepper");
}

static void mistakesAreSilly() {
    const Recipe& glow = recipes()[0];  // glow mushroom, moon water
    // Something that doesn't belong: its own mischief.
    Verdict v = judge(glow, {I::GlowMushroom, I::GummyNewt, I::MoonWater});
    CHECK(v.outcome == Outcome::Frog);
    CHECK(v.hasCulprit && v.culprit == I::GummyNewt);
    CHECK(v.message.find("Gummy Newt") != std::string::npos);
    // One too many of the right thing.
    v = judge(glow, {I::GlowMushroom, I::MoonWater, I::MoonWater});
    CHECK(v.outcome == Outcome::Floaty);
    CHECK(v.message.find("too much") != std::string::npos);
    // Not enough: a fizzle naming what is missing.
    v = judge(glow, {I::GlowMushroom});
    CHECK(v.outcome == Outcome::Fizzle);
    CHECK(v.culprit == I::MoonWater);
    // The right things, muddled: a burp.
    v = judge(glow, {I::MoonWater, I::GlowMushroom});
    CHECK(v.outcome == Outcome::Burp);
    // Every ingredient has its own mischief.
    std::set<int> seen;
    for (int i = 0; i < kIngredientCount; ++i) seen.insert(static_cast<int>(mischief(static_cast<I>(i))));
    CHECK(static_cast<int>(seen.size()) == kIngredientCount);
    CHECK(!seen.count(static_cast<int>(Outcome::Success)));
    // Too much beats too little: the extra is what shows.
    v = judge(recipes()[1], {I::EmberPepper, I::EmberPepper});
    CHECK(v.outcome == Outcome::Hiccups);
    // A riddle's fizzle doesn't give the answer away.
    const Recipe& dreams = recipes()[5];
    CHECK(dreams.riddle);
    v = judge(dreams, {I::MoonWater});
    CHECK(v.outcome == Outcome::Fizzle);
    CHECK(v.message.find("clues") != std::string::npos);
    for (int o = 0; o < kOutcomeCount; ++o) CHECK(*outcomeTitle(static_cast<Outcome>(o)));
}

static void colourMoves() {
    const Rgb water = brewColor({});
    const Rgb pumpkin = brewColor({I::PumpkinSeeds});
    CHECK(pumpkin.r > water.r + 0.4f);
    // Order changes the colour (the last spoonful pulls hardest after the first).
    const Rgb a = brewColor({I::PumpkinSeeds, I::MoonWater});
    const Rgb b = brewColor({I::MoonWater, I::PumpkinSeeds});
    CHECK(a.b > b.b);
    for (float c : {a.r, a.g, a.b, b.r, b.g, b.b}) CHECK(c >= 0.0f && c <= 1.0f);
    CHECK(matchedPrefix(recipes()[0], {I::GlowMushroom}) == 1);
    CHECK(matchedPrefix(recipes()[0], {I::MoonWater}) == 0);
}

static void anEvening() {
    Game g;
    CHECK(!g.stir());  // nothing in the pot
    CHECK(g.add(I::GummyNewt));
    CHECK(g.stir());
    CHECK(g.last.outcome == Outcome::Frog);
    CHECK(g.tries == 1);
    CHECK(!g.add(I::MoonWater));  // pour out first
    g.next();                     // no: not a success
    CHECK(g.recipe == 0);
    g.pourOut();
    CHECK(g.pot.empty() && !g.stirred);
    for (I i : recipes()[0].steps) CHECK(g.add(i));
    CHECK(g.stir());
    CHECK(g.last.outcome == Outcome::Success);
    g.pourOut();  // a finished potion isn't poured away
    CHECK(g.stirred);
    g.next();
    CHECK(g.recipe == 1);
    CHECK(g.triesPerRecipe.size() == 1 && g.triesPerRecipe[0] == 1);
    CHECK(g.totalStars() == 2);
    // The pot holds only so much.
    for (int i = 0; i < kCapacity; ++i) CHECK(g.add(I::StarSugar));
    CHECK(!g.add(I::StarSugar));
    g.pourOut();
    // Finish the book, first time every time from here.
    while (!g.done()) {
        CHECK(!g.canPeek());
        for (I i : recipes()[g.recipe].steps) g.add(i);
        CHECK(g.stir());
        g.next();
    }
    CHECK(g.totalStars() == 2 + 3 * (recipeCount() - 1));
    CHECK(!g.add(I::MoonWater));
    // A riddle offers its answer after two wrong stirs.
    Game h;
    h.recipe = 5;
    for (int k = 0; k < 2; ++k) {
        h.add(I::GummyNewt);
        h.stir();
        h.pourOut();
    }
    CHECK(h.canPeek());
    CHECK(Game::stars(0) == 3 && Game::stars(2) == 2 && Game::stars(5) == 1);
}

int main() {
    recipesAreSound();
    cardsReadNaturally();
    mistakesAreSilly();
    colourMoves();
    anEvening();
    TEST_MAIN_END();
}
