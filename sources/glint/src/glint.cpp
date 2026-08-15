#include <glint/glint.hpp>

#include <glint/core.hpp>

namespace glint {

Game::Game(const core::Config& config) : m_reg {} {
    init_core(m_reg, config);

    for (const auto& module : config.native_addons) {
        TraceLog(LOG_INFO, "GLINT: Registering addon '%s'", module.name.c_str());

        if (module.pre_init) m_pre_init.emplace_back(module.pre_init);
        if (module.init) m_init.emplace_back(module.init);
        if (module.post_init) m_post_init.emplace_back(module.post_init);

        if (module.pre_deinit) m_pre_deinit.emplace_back(module.pre_deinit);
        if (module.deinit) m_deinit.emplace_back(module.deinit);
        if (module.post_deinit) m_post_deinit.emplace_back(module.post_deinit);

        if (module.pre_update) m_pre_update.emplace_back(module.pre_update);
        if (module.update) m_update.emplace_back(module.update);
        if (module.post_update) m_post_update.emplace_back(module.post_update);

        if (module.pre_render) m_pre_render.emplace_back(module.pre_render);
        if (module.render) m_render.emplace_back(module.render);
        if (module.post_render) m_post_render.emplace_back(module.post_render);
    }

    call(m_pre_init);
    call(m_init);
    call(m_post_init);

    if (!config.initial_module.empty()) {
        auto initial = m_reg.create();
        m_reg.emplace<core::LuaComp>(initial, core::load_module(m_reg, config.initial_module));
    }
}

Game::~Game() {
    auto config = m_reg.ctx().get<core::Config>();

    call(m_pre_deinit);
    call(m_deinit);
    call(m_post_deinit);

    deinit_core(m_reg);
}

auto Game::registry() -> entt::registry& {
    return m_reg;
}

auto Game::registry() const -> const entt::registry& {
    return m_reg;
}

auto Game::run() -> int try {
    while (is_running()) {
        frame();
    }
    return 0;
} catch (const std::exception& e) {
    TraceLog(LOG_ERROR, "Exception occured while running game: %s", e.what());
    return 1;
}

auto Game::is_running() -> bool {
    return !WindowShouldClose();
}

auto Game::frame() -> void {
    update_core(m_reg);

    call(m_pre_update);
    call(m_update);
    call(m_post_update);

    BeginDrawing();
    ClearBackground(BLACK);

    call(m_pre_render);
    call(m_render);
    call(m_post_render);

    EndDrawing();
}

auto Game::call(CallbackList& list) -> void {
    for (auto& c : list) {
        c(m_reg);
    }
}

} // namespace glint

//
// #include <cstddef>
//
// #include <dsp/filter.hpp>
// #include <entt/entt.hpp>
// #include <raylib-cpp/raylib-cpp.hpp>
//
// #include <glint/assets.hpp>
// #include <glint/registry.hpp>
// #include <glint/systems.hpp>
//
// namespace glint {
//
// struct AudioState {
//     dsp::OnePoleFilter left_filter {};
//     dsp::OnePoleFilter right_filter {};
//
//     AudioState() {
//         left_filter.mode = dsp::FilterMode::LowPass;
//         right_filter.mode = dsp::FilterMode::LowPass;
//     }
// };
//
// static AudioState *global_audio_state = nullptr;
//
// static auto audio_callback(void *buffer_data, unsigned int frames) -> void {
//     if (global_audio_state == nullptr) {
//         TraceLog(LOG_WARNING, "Could not process audio: global_audio_state is null");
//         return;
//     }
//
//     auto buf = std::span<float> {static_cast<float *>(buffer_data), size_t(frames * 2)};
//     for (unsigned int i = 0; i < frames * 2; i += 2) {
//         buf[i] = global_audio_state->left_filter.process_single(buf[i]);
//         buf[i + 1] = global_audio_state->right_filter.process_single(buf[i + 1]);
//     }
// }
//
// auto init(entt::registry& reg, const Config& config) -> void {
//     set_registry(reg);
//
//     init_core(reg, config);
//     setup_assets(reg);
//
//     AttachAudioMixedProcessor(audio_callback);
//     reg.ctx().insert_or_assign<AudioState>(AudioState {});
//     global_audio_state = &reg.ctx().get<AudioState>();
//
//     auto game = reg.create();
//     reg.emplace<LuaComp>(game, load_module(reg, "game.lua"));
// }
//
// auto deinit(entt::registry& reg) -> void {
//     deinit_core(reg);
// }
//
// auto is_running(entt::registry& reg) -> bool {
//     (void)reg;
//     return !WindowShouldClose();
// }
//
// auto frame(entt::registry& reg) -> void {
//     update_assets(reg);
//     update_nuklear(reg);
//     update_sprites(reg);
//     update_lua(reg);
//
//     (void)reg;
//     BeginDrawing();
//     ClearBackground(GetColor(0xbb18bbff));
//     draw_sprites(reg);
//     render_nuklear(reg);
//     EndDrawing();
// }
//
// } // namespace glint
