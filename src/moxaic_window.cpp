#include "main.hpp"
#include "moxaic_window.hpp"

#include "moxaic_logging.hpp"

#include <vector>

SDL_Window *Moxaic::g_pSDLWindow;
VkExtent2D Moxaic::g_WindowDimensions;

std::vector<const Moxaic::MouseMotionCallback *> Moxaic::g_MouseMotionSubscribers;
std::vector<const Moxaic::MouseCallback *> Moxaic::g_MouseSubscribers;
std::vector<const Moxaic::KeyCallback *> Moxaic::g_KeySubscribers;

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
            case SDL_KEYDOWN: {
                KeyEvent event{
                        .phase = Phase::Pressed,
                        .key = pollEvent.key.keysym.sym,
                };
                for (int i = 0; i < g_KeySubscribers.size(); ++i) {
                    (*g_KeySubscribers[i])(event);
                }
                break;
            }
            case SDL_KEYUP: {
                KeyEvent event{
                        .phase = Phase::Released,
                        .key = pollEvent.key.keysym.sym,
                };
                for (int i = 0; i < g_KeySubscribers.size(); ++i) {
                    (*g_KeySubscribers[i])(event);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                MouseEvent event{
                        .phase = Phase::Pressed,
                        .button = static_cast<Button>(pollEvent.button.button)
                };
                for (int i = 0; i < g_MouseSubscribers.size(); ++i) {
                    (*g_MouseSubscribers[i])(event);
                }
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                MouseEvent event{
                        .phase = Phase::Released,
                        .button = static_cast<Button>(pollEvent.button.button)
                };
                for (int i = 0; i < g_MouseSubscribers.size(); ++i) {
                    (*g_MouseSubscribers[i])(event);
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                // not accumulating and eventing at the end might produce more correct behaviour
                MouseMotionEvent event{
                        .delta = glm::vec2((float) pollEvent.motion.xrel,
                                           (float) pollEvent.motion.yrel)
                };
                for (int i = 0; i < g_MouseMotionSubscribers.size(); ++i) {
                    (*g_MouseMotionSubscribers[i])(event);
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
