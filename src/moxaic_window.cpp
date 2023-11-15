#include "main.hpp"
#include "moxaic_window.hpp"

#include "moxaic_logging.hpp"

#include <glm/gtc/constants.hpp>

using namespace Moxaic;
using namespace Moxaic::Window;

SDL_Window *g_pSDLWindow;
VkExtent2D g_WindowDimensions;
UserCommand g_UserCommand;

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
            g_UserCommand.userMove.ToggleFlag(UserMove::Forward, pressed);
            break;
        case SDLK_s:
            g_UserCommand.userMove.ToggleFlag(UserMove::Back, pressed);
            break;
        case SDLK_a:
            g_UserCommand.userMove.ToggleFlag(UserMove::Left, pressed);
            break;
        case SDLK_d:
            g_UserCommand.userMove.ToggleFlag(UserMove::Right, pressed);
            break;
    }
}

const UserCommand &Window::userCommand()
{
    return g_UserCommand;
}

const VkExtent2D Window::extents()
{
    return g_WindowDimensions;
}

SDL_Window *Window::window()
{
    return g_pSDLWindow;
}

bool Window::Init()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    g_pSDLWindow = SDL_CreateWindow(k_ApplicationName,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    k_DefaultWidth, k_DefaultHeight,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    int x, y;
    SDL_GetWindowPosition(g_pSDLWindow, &x, &y);
    SDL_SetWindowPosition(g_pSDLWindow, x + (g_Role == Role::Compositor ? -(int)(k_DefaultWidth / 2) : (int)(k_DefaultWidth / 2)), y);
    g_WindowDimensions = VkExtent2D{k_DefaultWidth, k_DefaultHeight};
    return g_pSDLWindow != nullptr;
}

void Window::Poll()
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

void Window::Shutdown()
{
    SDL_DestroyWindow(g_pSDLWindow);
    SDL_Quit();
}
