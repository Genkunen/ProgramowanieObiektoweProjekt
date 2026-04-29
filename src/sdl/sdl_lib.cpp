#include "sdl_lib.hpp"

namespace pop::sdl {

void initializeSdl() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error("SDL could not initialize!");
    }
}

void terminateSdl() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
}

}
