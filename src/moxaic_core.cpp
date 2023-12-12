#include "moxaic_core.hpp"
#include "main.hpp"
#include "moxaic_scene.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_window.hpp"

#include <memory>

using namespace Moxaic;

MXC_RESULT Core::Run()
{
    MXC_CHK(Window::Init());
    MXC_CHK(Vulkan::Init(true));

    const auto device = std::make_unique<Vulkan::Device>();
    MXC_CHK(device->Init());

    Vulkan::CompositorPipelineType = Vulkan::PipelineType::Compute;

    std::unique_ptr<SceneBase> scene;
    switch (Role) {
        case Role::Compositor:
            if (Vulkan::CompositorPipelineType == Vulkan::PipelineType::Graphics) {
                scene = std::make_unique<CompositorScene>(device.get());
                break;
            }
            if (Vulkan::CompositorPipelineType == Vulkan::PipelineType::Compute) {
                scene = std::make_unique<ComputeCompositorScene>(device.get());
                break;
            }
            break;
        case Role::Node:
            scene = std::make_unique<NodeScene>(device.get());
            break;
    }
    scene->Init();

    Uint32 time = 0;
    Uint32 priorTime = 0;

    while (Running == MXC_SUCCESS) {
        time = SDL_GetTicks();
        const Uint32 deltaTime = time - priorTime;
        priorTime = time;

        Window::Poll();

        Running = scene->Loop(deltaTime);
    }

    Window::Cleanup();

    return MXC_SUCCESS;
}
