#include "main.hpp"
#include "moxaic_window.hpp"

#include "moxaic_logging.hpp"

#include <vector>

SDL_Window *Moxaic::g_pSDLWindow;
VkExtent2D Moxaic::g_WindowDimensions;

int g_DeltaRawMouseX;
int g_DeltaRawMouseY;
bool g_RelativeMouseMode;

std::vector<void (*)(Moxaic::MouseMotionEvent&)> Moxaic::g_MouseMotionSubscribers;

bool Moxaic::WindowInit()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    g_pSDLWindow = SDL_CreateWindow(k_ApplicationName,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    k_DefaultWidth, k_DefaultHeight,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    g_WindowDimensions = VkExtent2D{k_DefaultWidth, k_DefaultHeight};
    return g_pSDLWindow != nullptr;
}

void Moxaic::WindowPoll()
{
    g_DeltaRawMouseX = 0;
    g_DeltaRawMouseY = 0;

    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                g_ApplicationRunning = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    g_RelativeMouseMode = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    g_RelativeMouseMode = false;
                }
                break;
            case SDL_MOUSEMOTION:
                // accumulate in case there are multiples
                if (g_RelativeMouseMode) {
                    g_DeltaRawMouseX += event.motion.xrel;
                    g_DeltaRawMouseY += event.motion.yrel;
                }
                break;
        }
    }

    if (g_RelativeMouseMode) {
        auto mouseMotionEvent = (MouseMotionEvent) {
                .delta = glm::vec2((float)g_DeltaRawMouseX * k_MouseSensitivity, (float)g_DeltaRawMouseY * k_MouseSensitivity)
        };
        for (int i = 0; i < g_MouseMotionSubscribers.size(); ++i){
            g_MouseMotionSubscribers[i](mouseMotionEvent);
        }
    }
}

void Moxaic::WindowShutdown()
{
    SDL_DestroyWindow(g_pSDLWindow);
    SDL_Quit();
}
