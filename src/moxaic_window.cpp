#include "main.hpp"
#include "moxaic_window.hpp"

#include "moxaic_logging.hpp"

#include <glm/gtc/constants.hpp>

SDL_Window *Moxaic::g_pSDLWindow;
VkExtent2D Moxaic::g_WindowDimensions;
Moxaic::UserCommand g_UserCommand;

inline static void SetMouseButton(const int index, const bool pressed)
{
    switch (index) {
        case SDL_BUTTON_LEFT:
            g_UserCommand.leftMouseButtonPressed = pressed;
    }
}

inline static void SetKey(const SDL_Keycode keycode, const bool pressed)
{
    switch (keycode) {
        case SDLK_w:
            g_UserCommand.userMove.ToggleFlag(Moxaic::UserMove::Forward, pressed);
            break;
        case SDLK_s:
            g_UserCommand.userMove.ToggleFlag(Moxaic::UserMove::Back, pressed);
            break;
        case SDLK_a:
            g_UserCommand.userMove.ToggleFlag(Moxaic::UserMove::Left, pressed);
            break;
        case SDLK_d:
            g_UserCommand.userMove.ToggleFlag(Moxaic::UserMove::Right, pressed);
            break;
    }
}

const Moxaic::UserCommand &Moxaic::getUserCommand()
{
    return g_UserCommand;
}

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
    if (g_UserCommand.mouseMoved) {
        g_UserCommand.mouseDelta = glm::zero<glm::vec2>();
        g_UserCommand.mouseMoved = false;
    }

    static SDL_Event pollEvent;
    while (SDL_PollEvent(&pollEvent)) {
        switch (pollEvent.type) {
            case SDL_QUIT: {
                g_ApplicationRunning = false;
                break;
            }
            case SDL_KEYDOWN: {
                SetKey(pollEvent.key.keysym.sym, true);
                break;
            }
            case SDL_KEYUP: {
                SetKey(pollEvent.key.keysym.sym, false);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                SetMouseButton(pollEvent.button.button, true);
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                SetMouseButton(pollEvent.button.button, false);
                break;
            }
            case SDL_MOUSEMOTION: {
                // accumulate, multiples may come
                g_UserCommand.mouseDelta += glm::vec2((float) pollEvent.motion.xrel, (float) pollEvent.motion.yrel);
                g_UserCommand.mouseMoved = true;
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
