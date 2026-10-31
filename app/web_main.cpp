// Cauldron in a browser: the cottage on Strata's WebGL 2 backend
// (Emscripten), with the page's recipe card, shelf buttons and sounds
// (web/shell.html). The page calls the cauldron_* functions; the game asks
// the page for sounds through Module.sfx(name).
#include <cstdio>
#include <memory>
#include <string>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "play/cottage.h"
#include "sim/brew.h"
#include "strata/strata.hpp"

using namespace cauldron;

namespace {

struct App {
    std::unique_ptr<strata::Engine> engine;
    std::unique_ptr<play::Cottage> cottage;
    double last = 0;
    std::string json;
};

App* g = nullptr;

void frame(void*) {
    StrataEvent e;
    while (g->engine->pollEvent(e)) {
    }
    const double now = emscripten_get_now() / 1000.0;
    g->cottage->frame(now - g->last);
    g->last = now;
    g->engine->present();
}

void str(std::string& out, const std::string& s) {
    out += '"';
    for (char ch : s) {
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += ch;
        } else if (static_cast<unsigned char>(ch) < 0x20) {
            out += ' ';
        } else {
            out += ch;
        }
    }
    out += '"';
}

void key(std::string& out, const char* k) {
    if (out.size() > 1) out += ',';
    out += '"';
    out += k;
    out += "\":";
}

void strings(std::string& out, const std::vector<std::string>& v) {
    out += '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ',';
        str(out, v[i]);
    }
    out += ']';
}

void color(std::string& out, Rgb c) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "[%.3f,%.3f,%.3f]", static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b));
    out += buf;
}

void cue(const char* name) {
    std::string js = "if (Module.sfx) Module.sfx(";
    str(js, name);
    js += ")";
    emscripten_run_script(js.c_str());
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int cauldron_add(int i) {
    if (!g || i < 0 || i >= kIngredientCount) return 0;
    return g->cottage->add(static_cast<Ingredient>(i)) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int cauldron_stir() { return g && g->cottage->stir() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int cauldron_pour() { return g && g->cottage->pourOut() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int cauldron_next() { return g && g->cottage->next() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void cauldron_restart() {
    if (g) g->cottage->restart();
}
EMSCRIPTEN_KEEPALIVE void cauldron_peek() {
    if (g) g->cottage->peek();
}
// A tap on the canvas at (nx, ny), 0..1: the jar it added, or -1.
EMSCRIPTEN_KEEPALIVE int cauldron_tap(double nx, double ny) {
    return g ? g->cottage->tap(static_cast<float>(nx), static_cast<float>(ny)) : -1;
}
EMSCRIPTEN_KEEPALIVE void cauldron_cover(double top, double bottom) {
    if (g) g->cottage->setCover(static_cast<float>(top), static_cast<float>(bottom));
}
EMSCRIPTEN_KEEPALIVE void cauldron_resize(double w, double h) {
    if (g && w >= 1 && h >= 1) g->engine->resize(static_cast<int32_t>(w), static_cast<int32_t>(h));
}

// The shelf, for the page's buttons: [{"name","blurb","color":[r,g,b]}...] (linear RGB).
EMSCRIPTEN_KEEPALIVE const char* cauldron_ingredients() {
    if (!g) return "[]";
    std::string o = "[";
    for (int i = 0; i < kIngredientCount; ++i) {
        const IngredientInfo& in = info(static_cast<Ingredient>(i));
        if (i) o += ',';
        o += "{\"name\":";
        str(o, in.name);
        o += ",\"blurb\":";
        str(o, in.blurb);
        o += ",\"color\":";
        color(o, in.color);
        float nx = -1, ny = -1;
        g->cottage->jarOnScreen(static_cast<Ingredient>(i), nx, ny);
        char buf[64];
        std::snprintf(buf, sizeof buf, ",\"jar\":[%.4f,%.4f]}", static_cast<double>(nx), static_cast<double>(ny));
        o += buf;
    }
    o += ']';
    g->json = std::move(o);
    return g->json.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* cauldron_status() {
    if (!g) return "{}";
    const play::Cottage& c = *g->cottage;
    const Game& gm = c.game();
    std::string o = "{";
    key(o, "recipe");
    o += std::to_string(gm.recipe);
    key(o, "recipes");
    o += std::to_string(recipeCount());
    key(o, "done");
    o += gm.done() ? "true" : "false";
    if (!gm.done()) {
        const Recipe& r = recipes()[static_cast<size_t>(gm.recipe)];
        key(o, "name");
        str(o, r.name);
        key(o, "riddle");
        o += r.riddle ? "true" : "false";
        key(o, "lines");
        if (r.riddle) {
            std::vector<std::string> clues(r.clues.begin(), r.clues.end());
            strings(o, clues);
        } else {
            strings(o, cardLines(r));
        }
        key(o, "answer");
        strings(o, gm.peeked ? cardLines(r) : std::vector<std::string>{});
        key(o, "steps");
        o += std::to_string(r.steps.size());
        key(o, "matched");
        o += std::to_string(matchedPrefix(r, gm.pot));
        key(o, "color");
        color(o, r.color);
    }
    key(o, "pot");
    o += '[';
    for (size_t i = 0; i < gm.pot.size(); ++i) {
        if (i) o += ',';
        o += std::to_string(static_cast<int>(gm.pot[i]));
    }
    o += ']';
    key(o, "capacity");
    o += std::to_string(kCapacity);
    key(o, "stirring");
    o += c.stirring() ? "true" : "false";
    key(o, "stirred");
    o += gm.stirred ? "true" : "false";
    key(o, "success");
    o += gm.stirred && gm.last.outcome == Outcome::Success ? "true" : "false";
    key(o, "effect");
    str(o, c.effectName());
    key(o, "title");
    str(o, gm.stirred ? outcomeTitle(gm.last.outcome) : "");
    key(o, "message");
    str(o, gm.stirred ? gm.last.message : "");
    key(o, "tries");
    o += std::to_string(gm.tries);
    key(o, "canPeek");
    o += gm.canPeek() && !gm.peeked ? "true" : "false";
    key(o, "stars");
    o += std::to_string(gm.totalStars());
    key(o, "maxStars");
    o += std::to_string(3 * recipeCount());
    key(o, "starsEach");
    o += '[';
    for (size_t i = 0; i < gm.triesPerRecipe.size(); ++i) {
        if (i) o += ',';
        o += std::to_string(Game::stars(gm.triesPerRecipe[i]));
    }
    o += ']';
    key(o, "made");
    {
        std::vector<std::string> names;
        for (size_t i = 0; i < gm.triesPerRecipe.size(); ++i) names.push_back(recipes()[i].name);
        strings(o, names);
    }
    o += '}';
    g->json = std::move(o);
    return g->json.c_str();
}

}  // extern "C"

int main() {
    static App app;
    g = &app;
    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw < 1 || ch < 1) {
        cw = 1280;
        ch = 720;
    }
    strata::EngineDesc desc;
    desc.surface = STRATA_SURFACE_WINDOW;
    desc.width = static_cast<int32_t>(cw);
    desc.height = static_cast<int32_t>(ch);
    desc.title = "Cauldron";
    try {
        g->engine = strata::Engine::create(desc);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "cauldron: cannot start the renderer: %s\n", ex.what());
        std::string js = "if (Module.onRendererFailed) Module.onRendererFailed(";
        str(js, ex.what());
        js += ")";
        emscripten_run_script(js.c_str());
        return 1;
    }
    g->cottage = std::make_unique<play::Cottage>(*g->engine, cue);
    g->last = emscripten_get_now() / 1000.0;
    emscripten_run_script("if (Module.onGameReady) Module.onGameReady()");
    emscripten_set_main_loop_arg(frame, nullptr, 0, false);
    return 0;
}
