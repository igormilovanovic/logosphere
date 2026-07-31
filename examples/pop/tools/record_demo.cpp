// Records a scripted winning playthrough of the shipped Prince of Persia
// level as an animated SVG, using the same render_gui() drawing calls the
// live windowed app uses (render_gui.h) but backed by SvgDrawSurface
// instead of the engine's Metal-backed IDrawSurface -- so the recording is
// pixel-for-pixel the same visual logic as pop_gui, not a hand-drawn
// approximation. No GLFW/Metal dependency: builds under every
// LOGOSPHERE_PROFILE.
//
// The route mirrors tests/test_pop_movement.cpp's
// test_shipped_level_is_winnable (walk to the gap, running jump, cross
// the loose tile, hit the plate, fight the guard, reach the exit) using
// the level's documented column layout (level_one.h).
//
// Usage:
//   ./pop_record_demo [output.svg]

#include "game.h"
#include "level_one.h"
#include "render_gui.h"
#include "svg_draw_surface.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace pop;

constexpr double kSecondsPerFrame = 2.6;   // matches demo.svg's per-frame pacing

void capture(Game& g, std::vector<std::string>& frames) {
    SvgDrawSurface surface;
    render_gui(surface, g.lv, g.prince, &g.guard, seconds_remaining(*g.world));
    frames.push_back(surface.take_frame());
}

// Walks right/left via tick() until the Prince reaches target_x, capturing
// one frame per tick. Stops early once combat starts (in_sword_range) or
// the game ends -- bounded so a broken route cannot hang the tool.
void walk_to(Game& g, int target_x, std::vector<std::string>& frames, int budget = 200) {
    for (int i = 0; i < budget && !g.finished && g.prince.x != target_x &&
                    !in_sword_range(g.prince, g.guard); ++i) {
        const Action a = target_x > g.prince.x ? Action::Right : Action::Left;
        tick(g, a, CombatAction::Idle);
        capture(g, frames);
    }
}

// Fights until the guard falls (or the Prince does). GAME_DESIGN.md notes
// pure aggression loses this duel; mixing short riposte bursts with
// parries wins, hence the cadence below.
void duel(Game& g, std::vector<std::string>& frames, int budget = 200) {
    static const CombatAction kCadence[] = {
        CombatAction::Strike, CombatAction::Strike, CombatAction::Parry,
    };
    for (int i = 0; i < budget && !g.finished && g.guard.alive(); ++i) {
        tick(g, Action::None, kCadence[i % 3]);
        capture(g, frames);
    }
}

std::string assemble_svg(const std::vector<std::string>& frames, int content_w, int content_h) {
    const int chrome_h = 30;
    const int pad = 8;
    const int w = content_w + pad * 2;
    const int h = content_h + pad * 2 + chrome_h;
    const double total_dur = kSecondsPerFrame * static_cast<double>(frames.size());

    char header[1024];
    std::snprintf(header, sizeof(header),
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
        "viewBox=\"0 0 %d %d\" role=\"img\" "
        "aria-label=\"Windowed recording of Prince of Persia on Logosphere: "
        "the Prince jumps a spiked pit, opens a gate, beats a guard and escapes\">\n"
        "  <title>Prince of Persia on Logosphere -- windowed demo</title>\n"
        "  <rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" rx=\"8\" ry=\"8\" "
        "fill=\"#0d1117\" stroke=\"#30363d\" stroke-width=\"1\"/>\n"
        "  <path d=\"M0,%d h%d\" stroke=\"#30363d\" stroke-width=\"1\"/>\n"
        "  <rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" rx=\"8\" ry=\"8\" fill=\"#161b22\"/>\n"
        "  <rect x=\"0\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#161b22\"/>\n"
        "  <circle cx=\"19\" cy=\"15\" r=\"5.5\" fill=\"#ff5f56\"/>\n"
        "  <circle cx=\"38\" cy=\"15\" r=\"5.5\" fill=\"#ffbd2e\"/>\n"
        "  <circle cx=\"57\" cy=\"15\" r=\"5.5\" fill=\"#27c93f\"/>\n"
        "  <text x=\"%d\" y=\"19\" text-anchor=\"middle\" fill=\"#8b949e\" "
        "font-family=\"SFMono-Regular,Consolas,Liberation Mono,Menlo,monospace\" "
        "font-size=\"11\">pop --gui</text>\n",
        w, h, w, h,
        w, h,
        chrome_h, w,
        w, chrome_h,
        chrome_h / 2, w, chrome_h - chrome_h / 2,
        w / 2);

    std::string svg(header);

    for (size_t i = 0; i < frames.size(); ++i) {
        const double start = static_cast<double>(i) * kSecondsPerFrame / total_dur;
        const double end = static_cast<double>(i + 1) * kSecondsPerFrame / total_dur;
        char group_open[512];
        std::snprintf(group_open, sizeof(group_open),
            "<g opacity=\"0\"><animate attributeName=\"opacity\" dur=\"%.2fs\" "
            "repeatCount=\"indefinite\" calcMode=\"discrete\" "
            "keyTimes=\"0;%.5f;%.5f;1\" values=\"0;1;0;0\"/>\n"
            "<g transform=\"translate(%d,%d)\">",
            total_dur, start, end, pad, pad + chrome_h);
        svg += group_open;
        svg += frames[i];
        svg += "</g></g>\n";
    }

    svg += "</svg>\n";
    return svg;
}

} // namespace

int main(int argc, char** argv) {
    const std::string out_path = argc > 1 ? argv[1] : "examples/pop/demo_gui.svg";

    Game g = new_game();
    std::vector<std::string> frames;
    capture(g, frames);                       // frame 0: the opening position

    walk_to(g, 3, frames);                    // to the lip of the gap (level_one.h: gap at col 4)
    tick(g, Action::Jump, CombatAction::Idle);
    capture(g, frames);                       // jumped

    walk_to(g, 15, frames);                   // cross the loose tile, drink the potion (col 15)
    walk_to(g, 18, frames);                   // hit the pressure plate, gate opens (col 18)
    walk_to(g, kGuardStartX, frames);         // advance until swords cross (guard at col 27)
    duel(g, frames);                          // the fight
    walk_to(g, 30, frames);                   // to the exit (col 30)

    if (!g.finished || g.prince.state != PrinceState::Escaped) {
        std::cerr << "[pop_record_demo] scripted route did not win -- "
                     "adjust the route in tools/record_demo.cpp\n";
        return 1;
    }

    const int content_w = gui_frame_width(g.lv);
    const int content_h = gui_frame_height(g.lv);
    const std::string svg = assemble_svg(frames, content_w, content_h);

    std::ofstream out(out_path);
    if (!out) {
        std::cerr << "[pop_record_demo] could not open " << out_path << " for writing\n";
        return 1;
    }
    out << svg;
    std::cout << "[pop_record_demo] wrote " << frames.size() << " frames to " << out_path << "\n";
    return 0;
}
