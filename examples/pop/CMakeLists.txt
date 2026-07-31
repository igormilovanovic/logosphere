cmake_minimum_required(VERSION 3.10)

# Prince of Persia: headless/console example. Links only logosphere_core
# (no rendering, GLFW, or Metal), so it builds under every
# LOGOSPHERE_PROFILE (core/physics/full) -- unlike examples/eden,
# examples/logotron, and examples/logogenesis, which need the full macOS
# engine. See POP.md for the windowed macOS path.
add_executable(pop
    src/main.cpp
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
