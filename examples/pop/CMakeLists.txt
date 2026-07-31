cmake_minimum_required(VERSION 3.10)

# Prince of Persia: headless/console example. Links only logosphere_core
# (no rendering, GLFW, or Metal), so it builds under every
# LOGOSPHERE_PROFILE (core/physics/full) -- unlike examples/eden,
# examples/logotron, and examples/logogenesis, which need the full macOS
# engine. See POP.md for the windowed macOS path.
add_executable(pop
    src/main.cpp
    src/game.cpp
    src/level.cpp
    src/prince.cpp
    src/combat.cpp
    src/render_ascii.cpp
    src/world.cpp
    src/generated/pop_ontology_registry.cpp
)

target_include_directories(pop PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pop PRIVATE logosphere_core)
target_compile_features(pop PRIVATE cxx_std_17)

set_target_properties(pop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pop
)

# pop_record_demo: replays a scripted winning route through game.h and
# writes an animated SVG (render_gui.h drawn via SvgDrawSurface instead of
# the engine's Metal-backed IDrawSurface). No GLFW/Metal dependency, so it
# builds in any profile, but it only exists to illustrate pop_gui's look,
# so it's kept alongside that target under LOGOSPHERE_FULL.
#
# pop_gui: the windowed macOS counterpart to the console `pop` target.
# Draws the same simulation through render_gui.h against the engine's real
# IDrawSurface. See POP.md "Windowed (macOS) version".
if(LOGOSPHERE_FULL)
    add_executable(pop_record_demo
        tools/record_demo.cpp
        src/game.cpp
        src/level.cpp
        src/prince.cpp
        src/combat.cpp
        src/render_ascii.cpp
        src/render_gui.cpp
        src/svg_draw_surface.cpp
        src/world.cpp
        src/generated/pop_ontology_registry.cpp
    )
    target_include_directories(pop_record_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(pop_record_demo PRIVATE logosphere_core)
    target_compile_features(pop_record_demo PRIVATE cxx_std_17)
    set_target_properties(pop_record_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pop
    )

    add_executable(pop_gui
        src/main_gui.cpp
        src/pop_app.cpp
        src/render_gui.cpp
        src/render_ascii.cpp
        src/game.cpp
        src/level.cpp
        src/prince.cpp
        src/combat.cpp
        src/world.cpp
        src/generated/pop_ontology_registry.cpp
    )
    target_include_directories(pop_gui PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(pop_gui PRIVATE
        logosphere
        glfw
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreGraphics"
        "-framework AppKit"
        "-framework Foundation"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit"
        "-framework MetalPerformanceShaders"
    )
    target_compile_features(pop_gui PRIVATE cxx_std_17)
    set_target_properties(pop_gui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pop
    )
endif()
