#include "vulkan/vk_context.hpp"

#include <SDL3/SDL.h>

auto main() -> int {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("ProgramowanieObiektoweProjekt", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    auto vulkan_context = pop::vulkan::VulkanContext::create(window);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
    }

    SDL_DestroyWindow(window);

    SDL_Quit();
}