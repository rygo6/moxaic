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

    std::unique_ptr<CompositorScene> compositorScene;
    std::unique_ptr<NodeScene> nodeScene;
    switch (g_Role) {
        case Role::Compositor:
            compositorScene = std::make_unique<CompositorScene>(*device);
            compositorScene->Init();
            break;
        case Role::Node:
            nodeScene = std::make_unique<NodeScene>(*device);
            nodeScene->Init();
            break;
    }

    Uint32 time = 0;
    Uint32 priorTime = 0;

    while (g_ApplicationRunning == MXC_SUCCESS) {

        time = SDL_GetTicks();
        Uint32 deltaTime = time - priorTime;
        priorTime = time;

        Window::Poll();

        switch (g_Role) {
            case Role::Compositor:
                g_ApplicationRunning = compositorScene->Loop(deltaTime);
                break;
            case Role::Node:
                g_ApplicationRunning = nodeScene->Loop(deltaTime);
                break;
        }
    }

    Window::Shutdown();

    return MXC_SUCCESS;
}