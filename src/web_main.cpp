#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include <array>
#include <stdexcept>
#include <string>

namespace {

SDL_Window* window{};
SDL_Renderer* renderer{};
SDL_Texture* render_target{};

void shutdown() {
    if (render_target != nullptr) {
        SDL_DestroyTexture(render_target);
        render_target = nullptr;
    }
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

void fail(const char* operation) {
    shutdown();
    throw std::runtime_error(
        std::string(operation) + ": " + SDL_GetError()
    );
}

void frame() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            emscripten_cancel_main_loop();
            shutdown();
            return;
        }
    }

    SDL_SetRenderTarget(renderer, render_target);
    SDL_SetRenderDrawColor(renderer, 24, 32, 43, 255);
    SDL_RenderClear(renderer);
    const SDL_Rect clip{24, 24, 208, 96};
    SDL_SetRenderClipRect(renderer, &clip);
    const std::array<SDL_Vertex, 3> vertices{{
        {{32.0F, 104.0F}, {0.20F, 0.75F, 0.48F, 1.0F}, {0.0F, 0.0F}},
        {{128.0F, 28.0F}, {0.82F, 0.72F, 0.24F, 1.0F}, {0.0F, 0.0F}},
        {{224.0F, 104.0F}, {0.35F, 0.56F, 0.92F, 1.0F}, {0.0F, 0.0F}},
    }};
    SDL_RenderGeometry(
        renderer, nullptr, vertices.data(), vertices.size(), nullptr, 0
    );
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_SetRenderTarget(renderer, nullptr);

    SDL_SetRenderDrawColor(renderer, 8, 12, 18, 255);
    SDL_RenderClear(renderer);
    SDL_FRect destination{0.0F, 0.0F, 256.0F, 144.0F};
    SDL_RenderTexture(renderer, render_target, nullptr, &destination);
    SDL_SetRenderDrawColor(renderer, 230, 236, 242, 255);
    SDL_RenderDebugText(renderer, 32.0F, 116.0F, "BROWSER RISK SPIKE");
    SDL_RenderPresent(renderer);
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fail("SDL_Init");
    }
    if (!SDL_CreateWindowAndRenderer(
            "AoE Browser Risk Spike",
            1280,
            720,
            SDL_WINDOW_RESIZABLE,
            &window,
            &renderer
        )) {
        fail("SDL_CreateWindowAndRenderer");
    }
    if (!SDL_SetRenderLogicalPresentation(
            renderer,
            1280,
            720,
            SDL_LOGICAL_PRESENTATION_LETTERBOX
        )) {
        fail("SDL_SetRenderLogicalPresentation");
    }
    render_target = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        256,
        144
    );
    if (render_target == nullptr) {
        fail("SDL_CreateTexture");
    }
    emscripten_set_main_loop(frame, 0, false);
    return 0;
}
