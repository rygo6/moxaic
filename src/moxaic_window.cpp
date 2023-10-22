#include "main.hpp"
#include "moxaic_window.hpp"

SDL_Window *Moxaic::g_pSDLWindow;

bool Moxaic::WindowInit()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    g_pSDLWindow = SDL_CreateWindow(k_ApplicationName,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    800, 600,
                                   SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    return g_pSDLWindow != nullptr;
}

void Moxaic::WindowPoll()
{
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_ApplicationRunning = false;
        }
    }
}

void Moxaic::WindowShutdown()
{
    SDL_DestroyWindow(g_pSDLWindow);
    SDL_Quit();
}
