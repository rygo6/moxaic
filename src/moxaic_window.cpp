#include "moxaic_window.hpp"
#include "main.hpp"
#include "moxaic_logging.hpp"

#include <SDL2/SDL_Vulkan.h>
#include <glm/gtc/constants.hpp>

using namespace Moxaic;
using namespace Moxaic::Window;

SDL_Window* g_pSDLWindow;
VkExtent2D g_WindowDimensions;
UserCommand g_UserCommand;

static void SetMouseButton(int const index, bool const pressed)
{
    switch (index) {
        case SDL_BUTTON_LEFT:
            g_UserCommand.leftMouseButtonPressed = pressed;
        default:;
    }
}

static void SetKey(SDL_Keycode const keycode, bool const pressed)
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
        default:;
    }
}

UserCommand const& Window::userCommand()
{
    return g_UserCommand;
}

VkExtent2D const& Window::extents()
{
    return g_WindowDimensions;
}

SDL_Window const* Window::window()
{
    return g_pSDLWindow;
}

MXC_RESULT Window::Init()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    g_pSDLWindow = SDL_CreateWindow(ApplicationName,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    DefaultWidth,
                                    DefaultHeight,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    int x, y;
    SDL_GetWindowPosition(g_pSDLWindow, &x, &y);
    SDL_SetWindowPosition(g_pSDLWindow,
                          x + (Role == Role::Compositor ?
                                 -(int) (DefaultWidth / 2) :
                                 (int) (DefaultWidth / 2)),
                          y);
    g_WindowDimensions = VkExtent2D{DefaultWidth, DefaultHeight};
    return g_pSDLWindow != nullptr;
}

MXC_RESULT Window::InitSurface(VkInstance const& vkInstance,
                               VkSurfaceKHR* pVkSurface)
{
    SDL_assert((g_pSDLWindow != nullptr) && "Window not initialized!");
    // should surface move into swap class?! or device class?
    MXC_CHK(SDL_Vulkan_CreateSurface(g_pSDLWindow,
                                     vkInstance,
                                     pVkSurface));
    return MXC_SUCCESS;
}

std::vector<char const*> Window::GetVulkanInstanceExtentions()
{
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(g_pSDLWindow, &extensionCount, nullptr);
    std::vector<char const*> requiredInstanceExtensionsNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(g_pSDLWindow, &extensionCount, requiredInstanceExtensionsNames.data());
    return requiredInstanceExtensionsNames;// rvo deals with this right?
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
                Running = false;
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

void Window::Cleanup()
{
    SDL_DestroyWindow(g_pSDLWindow);
    SDL_Quit();
}
