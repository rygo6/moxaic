#include "moxaic_window.hpp"
#include "main.hpp"
#include "moxaic_logging.hpp"

#include <SDL2/SDL_Vulkan.h>
#include <glm/gtc/constants.hpp>

using namespace Moxaic;
using namespace Moxaic::Window;


SDL_Window* window;
VkExtent2D extents{800, 600};
UserCommand userCommand;

static void SetMouseButton(const int index, const bool pressed)
{
    switch (index) {
        case SDL_BUTTON_LEFT:
            userCommand.leftMouseButtonPressed = pressed;
        default:;
    }
}

static void SetKey(const SDL_Keycode keycode, const bool pressed)
{
    switch (keycode) {
        case SDLK_w:
            userCommand.userMove.ToggleFlag(UserMove::Forward, pressed);
            break;
        case SDLK_s:
            userCommand.userMove.ToggleFlag(UserMove::Back, pressed);
            break;
        case SDLK_a:
            userCommand.userMove.ToggleFlag(UserMove::Left, pressed);
            break;
        case SDLK_d:
            userCommand.userMove.ToggleFlag(UserMove::Right, pressed);
            break;
        default:;
    }
}

const UserCommand& Window::GetUserCommand()
{
    return userCommand;
}

const VkExtent2D& Window::GetExtents()
{
    return extents;
}

const SDL_Window* Window::GetWindow()
{
    return window;
}

MXC_RESULT Window::Init()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    window = SDL_CreateWindow(ApplicationName,
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    DefaultWidth,
                                    DefaultHeight,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    int x, y;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_SetWindowPosition(window,
                          x + (Role == Role::Compositor ?
                                 -(int) (DefaultWidth / 2) :
                                 (int) (DefaultWidth / 2)),
                          y);
    extents = VkExtent2D{DefaultWidth, DefaultHeight};
    return window != nullptr;
}

MXC_RESULT Window::InitSurface(VkInstance vkInstance,
                               VkSurfaceKHR* pVkSurface)
{
    SDL_assert((window != nullptr) && "Window not initialized!");
    // should surface move into swap class?! or device class?
    MXC_CHK(SDL_Vulkan_CreateSurface(window,
                                     vkInstance,
                                     pVkSurface));
    return MXC_SUCCESS;
}

std::vector<const char*> Window::GetVulkanInstanceExtentions()
{
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> requiredInstanceExtensionsNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, requiredInstanceExtensionsNames.data());
    return requiredInstanceExtensionsNames;// rvo deals with this right?
}

void Window::Poll()
{
    if (userCommand.mouseMoved) {
        userCommand.mouseDelta = glm::zero<glm::vec2>();
        userCommand.mouseMoved = false;
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
                userCommand.mouseDelta += glm::vec2((float) pollEvent.motion.xrel, (float) pollEvent.motion.yrel);
                userCommand.mouseMoved = true;
                break;
            }
        }
    }
}

void Window::Cleanup()
{
    SDL_DestroyWindow(window);
    SDL_Quit();
}
