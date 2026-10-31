// The rules of the cauldron: what can go in, the recipes, and what a brew
// turns into when it is stirred. No engine, no GPU: it builds and tests in
// seconds (tests/test_brew.cpp).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cauldron {

struct Rgb {
    float r = 0, g = 0, b = 0;
};

enum class Ingredient : uint8_t {
    GummyNewt,
    GlowMushroom,
    MoonWater,
    BatWing,
    PumpkinSeeds,
    EmberPepper,
    StarSugar,
    SpiderSilk,
};
constexpr int kIngredientCount = 8;

struct IngredientInfo {
    const char* name;    // "Gummy Newt"
    const char* plural;  // "Gummy Newts"
    const char* blurb;   // a few words for the shelf label
    Rgb color;           // linear RGB: the jar, the drop and what it does to the brew
};
const IngredientInfo& info(Ingredient i);

// What happens when the brew is stirred.
enum class Outcome : uint8_t {
    Success,
    // Something went in that shouldn't have, or too much of it: the culprit
    // decides the mischief.
    Frog,        // gummy newt: a frog hops out and ribbits
    Disco,       // glow mushroom: the room flashes every colour
    Floaty,      // moon water: the cauldron floats up and bobs
    BubbleBats,  // bat wing: bubbles flap around the room
    PolkaDots,   // pumpkin seeds: the whole room goes polka-dot
    Hiccups,     // ember pepper: the cauldron hiccups puffs of smoke
    Sprinkles,   // star sugar: sprinkles rain from the ceiling
    BigBubble,   // spider silk: a giant bubble grows and goes boing
    // The right things, but not enough of one: a sad grey fizzle.
    Fizzle,
    // The right things in the wrong order: a big burp and a smoke ring.
    Burp,
};
constexpr int kOutcomeCount = 11;

// The mischief an ingredient makes when it is one too many.
Outcome mischief(Ingredient i);
// A short name for an outcome ("A frog!") and a line about it.
const char* outcomeTitle(Outcome o);

// How a finished potion shows off.
enum class Flourish : uint8_t {
    Fountain,  // sparkles in the potion's colour
    Moonrise,  // a little glowing moon rises out of the pot
    FrogKing,  // a happy frog in a crown
    Lanterns,  // jack-o'-lanterns light up around the room
    Starfall,  // golden stars drift down
    Wings,     // a flock of friendly bats loops the room
};

struct Recipe {
    const char* name;
    std::vector<Ingredient> steps;  // in order; a repeat is another spoonful
    bool riddle = false;            // the card shows clues, not the steps
    std::vector<const char*> clues; // riddle cards only
    Rgb color;                      // the finished potion
    Flourish flourish = Flourish::Fountain;
    const char* result;             // what the finished potion does
};

// Every recipe, easiest first. The last few are riddles.
const std::vector<Recipe>& recipes();
int recipeCount();

// The card's lines for a step list: "2 Pumpkin Seeds", "then 1 Ember Pepper".
std::vector<std::string> cardLines(const Recipe& r);

// The most spoonfuls a cauldron holds.
constexpr int kCapacity = 10;

struct Verdict {
    Outcome outcome = Outcome::Success;
    // The ingredient to blame (for Frog..BigBubble and Fizzle), if any.
    bool hasCulprit = false;
    Ingredient culprit = Ingredient::GummyNewt;
    std::string message;  // a kind line for the player, with a nudge
};

// Judges what is in the pot against a recipe.
Verdict judge(const Recipe& r, const std::vector<Ingredient>& added);

// The brew's colour after these spoonfuls: plain water-dark at first,
// each spoonful pulling it towards its own colour.
Rgb brewColor(const std::vector<Ingredient>& added);

// How far through the card the player is: the number of leading spoonfuls
// that match the recipe (the card ticks these off).
int matchedPrefix(const Recipe& r, const std::vector<Ingredient>& added);

// The whole evening: which recipe, what is in the pot, how it went.
struct Game {
    int recipe = 0;                  // index into recipes(); == recipeCount() when the book is done
    std::vector<Ingredient> pot;
    int tries = 0;                   // stirs of this recipe that went wrong
    std::vector<int> triesPerRecipe; // one per finished recipe
    bool stirred = false;            // the last stir's verdict is showing
    Verdict last;
    bool peeked = false;             // the answer of a riddle was shown

    bool done() const { return recipe >= recipeCount(); }
    // Puts a spoonful in; false (and nothing changes) if the pot is full,
    // the book is done, or a verdict is showing (pour out or go on first).
    bool add(Ingredient i);
    // Stirs: judges the pot. False if the pot is empty or a verdict is showing.
    bool stir();
    // Empties the pot (after a mishap, or any time); the verdict goes.
    void pourOut();
    // After a success: on to the next recipe.
    void next();
    // The answer for a riddle, offered after two wrong stirs.
    bool canPeek() const;
    // Stars for a finished recipe: 3 first time, 2 within three tries, else 1.
    static int stars(int tries);
    int totalStars() const;
};

}  // namespace cauldron
