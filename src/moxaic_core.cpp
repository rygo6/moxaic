#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_window.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_scene.hpp"

#include <memory>

using namespace Moxaic;

MXC_RESULT Core::Run()
{
    MXC_CHK(Window::Init());
    MXC_CHK(Vulkan::Init(Window::window(), true));

    auto device = std::make_unique<Vulkan::Device>();
    MXC_CHK(device->Init());

    auto scene = std::make_unique<Scene>(*device);

    if (g_Role == Role::Compositor) {
        MXC_CHK(scene->Init());
    }

    Uint32 time = 0;
    Uint32 priorTime = 0;

    while (g_ApplicationRunning) {

        time = SDL_GetTicks();
        Uint32 deltaTime = time - priorTime;
        priorTime = time;

        Window::Poll();

        if (g_Role == Role::Compositor) {
            scene->Loop(deltaTime);
        }
    }

    Window::Shutdown();

    return MXC_SUCCESS;
}