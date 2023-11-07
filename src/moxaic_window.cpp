#include "main.hpp"
#include "moxaic_window.hpp"

#include "moxaic_logging.hpp"

#include <vector>

SDL_Window *Moxaic::g_pSDLWindow;
VkExtent2D Moxaic::g_WindowDimensions;

std::vector<void (*)(const Moxaic::MouseMotionEvent &)> Moxaic::g_MouseMotionSubscribers;
std::vector<void (*)(const Moxaic::MouseClickEvent &)> Moxaic::g_MouseClickSubscribers;

bool g_RelativeMouseMode;

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
    static SDL_Event pollEvent;
    while (SDL_PollEvent(&pollEvent)) {
        switch (pollEvent.type) {
            case SDL_QUIT: {
                g_ApplicationRunning = false;
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                MouseClickEvent event{
                        .phase = Phase::Pressed,
                        .button = static_cast<Button>(pollEvent.button.button)
                };
                for (int i = 0; i < g_MouseClickSubscribers.size(); ++i) {
                    g_MouseClickSubscribers[i](event);
                }
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                MouseClickEvent event{
                        .phase = Phase::Released,
                        .button = static_cast<Button>(pollEvent.button.button)
                };
                for (int i = 0; i < g_MouseClickSubscribers.size(); ++i) {
                    g_MouseClickSubscribers[i](event);
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                // not accumulating and eventing at the end might produce more correct behaviour
                MouseMotionEvent event{
                        .delta = glm::vec2((float) pollEvent.motion.xrel * k_MouseSensitivity,
                                           (float) pollEvent.motion.yrel * k_MouseSensitivity)
                };
                for (int i = 0; i < g_MouseMotionSubscribers.size(); ++i) {
                    g_MouseMotionSubscribers[i](event);
                }
                break;
            }
        }
    }
}

void Moxaic::WindowShutdown()
{
    SDL_DestroyWindow(g_pSDLWindow);
    SDL_Quit();
}
