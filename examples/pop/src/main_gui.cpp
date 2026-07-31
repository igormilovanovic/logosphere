// Prince of Persia on Logosphere - windowed macOS entry point.
//
// All game logic lives in game.h; this only wires PopApp into the
// engine's window/Metal loop, following examples/logogenesis/src/main.cpp.
// See POP.md for how this differs from the headless src/main.cpp.
#include "level_one.h"
#include "pop_app.h"
#include "render_gui.h"

#include "core/engine.h"

#include <chrono>
#include <iostream>

int main() {
    pop::PopApp app;
    Engine engine(&app);

    // The window is sized to the shipped level's tile grid plus the HUD
    // strip -- see render_gui.h -- rather than the engine's 1600x1200
    // default scene size.
    const pop::Level lv = pop::level_one();

    EngineConfig config;
    config.create_display = true;
    config.window_width = pop::gui_frame_width(lv);
    config.window_height = pop::gui_frame_height(lv);
    config.window_title = "Prince of Persia on Logosphere";
    config.show_debug_overlay = false;
    config.show_kg_inspector = false;

    if (engine.initialize(config) < 0) {
        std::cerr << "[pop] engine init failed" << std::endl;
        return 1;
    }

    auto last = std::chrono::high_resolution_clock::now();
    while (engine.should_continue()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;   // hitch guard

        engine.update(dt);
        engine.render();
        engine.present();
    }

    engine.shutdown();
    return 0;
}
