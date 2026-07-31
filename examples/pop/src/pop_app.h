// PopApp: windowed macOS frontend for the Prince of Persia example.
//
// Thin by design: every game rule lives in the tiers this reuses
// unchanged (game.h -> level.h/prince.h/combat.h/world.h). This class
// only owns a Game, maps key presses onto the same Action/CombatAction
// the console loop uses, and calls render_gui() against the engine's
// IDrawSurface each frame. See POP.md "Windowed (macOS) version".
#pragma once

#include "application.h"
#include "game.h"

class Engine;

namespace pop {

class PopApp : public Logosphere::IApplication {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    GLFWwindow* get_window() override { return nullptr; }

    void initialize_game(void* engine_ptr) override;
    void render_game() override;
    bool handle_key(int key, int scancode, int action, int mods) override;

    const char* get_app_name() const override { return "Prince of Persia on Logosphere"; }

private:
    void do_tick(Action action, CombatAction combat_action);

    Engine* engine_ = nullptr;
    Game game_;
};

} // namespace pop
