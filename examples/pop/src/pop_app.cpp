#include "pop_app.h"

#include "render_gui.h"

#include "core/engine.h"

#include <GLFW/glfw3.h>

#include <iostream>

namespace pop {

void PopApp::initialize_game(void* engine_ptr) {
    engine_ = static_cast<Engine*>(engine_ptr);
    game_ = new_game();
}

void PopApp::render_game() {
    if (!engine_) return;
    render_gui(engine_->get_draw_surface(), game_.lv, game_.prince, &game_.guard,
               seconds_remaining(*game_.world));
}

void PopApp::do_tick(Action action, CombatAction combat_action) {
    tick(game_, action, combat_action, [](const std::string& text) {
        std::cout << "  " << text << "\n";
    });
}

bool PopApp::handle_key(int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS) return false;
    if (game_.finished) return false;

    switch (key) {
        case GLFW_KEY_A:      do_tick(Action::Left,  CombatAction::Idle);   return true;
        case GLFW_KEY_D:      do_tick(Action::Right, CombatAction::Idle);   return true;
        case GLFW_KEY_W:      do_tick(Action::Up,    CombatAction::Idle);   return true;
        case GLFW_KEY_S:      do_tick(Action::Down,  CombatAction::Idle);   return true;
        case GLFW_KEY_SPACE:
        case GLFW_KEY_J:      do_tick(Action::Jump,   CombatAction::Idle);  return true;
        case GLFW_KEY_K:      do_tick(Action::Attack, CombatAction::Strike); return true;
        case GLFW_KEY_P:      do_tick(Action::Block,  CombatAction::Parry); return true;
        case GLFW_KEY_PERIOD: do_tick(Action::None,   CombatAction::Idle);  return true;
        default: return false;
    }
}

} // namespace pop
