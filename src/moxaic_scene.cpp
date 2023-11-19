#include "moxaic_scene.hpp"

#include <thread>
#include <chrono>

using namespace Moxaic;

MXC_RESULT CompositorScene::Init()
{
    MXC_CHK(framebuffer.Init(Window::extents(),
                             Vulkan::Locality::Local));

    camera.transform().setPosition({0, 0, -2});
    camera.transform().Rotate(0, 180, 0);
    camera.UpdateView();

    transform.setPosition({2, 0, 0});

    MXC_CHK(globalDescriptor.Init(camera,
                                  Window::extents()));

    MXC_CHK(texture.InitFromFile("textures/test.jpg",
                                 Vulkan::Locality::Local));
    MXC_CHK(texture.TransitionImmediateInitialToGraphicsRead());

    MXC_CHK(materialDescriptor.Init(texture));

    MXC_CHK(objectDescriptor.Init(transform));

    MXC_CHK(standardPipeline.Init(globalDescriptor,
                                  materialDescriptor,
                                  objectDescriptor));

    MXC_CHK(swap.Init(Window::extents(),
                      false));

    MXC_CHK(semaphore.Init(true,
                           Vulkan::Locality::External));

    MXC_CHK(mesh.Init());

    MXC_CHK(compositorNode.Init());

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds (200));
    MXC_CHK(compositorNode.ExportOverIPC(semaphore));

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(const uint32_t deltaTime)
{
    if (camera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        globalDescriptor.UpdateView(camera);
    }

    k_Device.BeginGraphicsCommandBuffer();
    k_Device.BeginRenderPass(framebuffer);

    standardPipeline.BindPipeline();
    standardPipeline.BindDescriptor(globalDescriptor);
    standardPipeline.BindDescriptor(materialDescriptor);
    standardPipeline.BindDescriptor(objectDescriptor);

    mesh.RecordRender();

    k_Device.EndRenderPass();

    swap.BlitToSwap(framebuffer.colorTexture());

    k_Device.EndGraphicsCommandBuffer();

    k_Device.SubmitGraphicsQueueAndPresent(semaphore, swap);

    semaphore.Wait();

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Init()
{
    MXC_CHK(node.Init());

    transform.setPosition({0, 0, 0});

    MXC_CHK(globalDescriptor.Init(camera,
                                  Window::extents()));

    MXC_CHK(texture.InitFromFile("textures/test.jpg",
                                 Vulkan::Locality::Local));
    MXC_CHK(texture.TransitionImmediateInitialToGraphicsRead());

    MXC_CHK(materialDescriptor.Init(texture));

    MXC_CHK(objectDescriptor.Init(transform));

    MXC_CHK(standardPipeline.Init(globalDescriptor,
                                  materialDescriptor,
                                  objectDescriptor));

    MXC_CHK(mesh.Init());

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Loop(const uint32_t deltaTime)
{
    node.ipcFromCompositor().Deque();

    return MXC_SUCCESS;
}
