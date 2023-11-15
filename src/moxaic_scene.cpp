#include "moxaic_scene.hpp"

using namespace Moxaic;

Scene::Scene(const Vulkan::Device &device)
        : device(device) {}

MXC_RESULT Scene::Init()
{
    MXC_CHK(framebuffer.Init(Window::extents(),
                             Vulkan::Locality::Local));

    camera.transform().setPosition({0, 0, -2});
    camera.transform().Rotate(0, 180, 0);
    camera.UpdateView();

    MXC_CHK(globalDescriptor.Init(camera,
                                  Window::extents()));

    MXC_CHK(texture.InitFromFile("textures/test.jpg",
                                 Vulkan::Locality::Local));
    MXC_CHK(texture.TransitionImmediateInitialToGraphicsRead());

    MXC_CHK(materialDescriptor.Init(texture));

    transform.setPosition({0, 0, 4});

    MXC_CHK(objectDescriptor.Init(transform));

    MXC_CHK(standardPipeline.Init(globalDescriptor,
                                  materialDescriptor,
                                  objectDescriptor));

    MXC_CHK(swap.Init(Window::extents(),
                      false));

    MXC_CHK(timelineSemaphore.Init(true,
                                   Vulkan::Locality::External));

    MXC_CHK(mesh.Init());

    MXC_CHK(compositorNode.Init());

    return MXC_SUCCESS;
}


MXC_RESULT Scene::Loop(const uint32_t deltaTime)
{
    if (camera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        globalDescriptor.UpdateView(camera);
    }

    device.BeginGraphicsCommandBuffer();
    device.BeginRenderPass(framebuffer);

    standardPipeline.BindPipeline();
    standardPipeline.BindDescriptor(globalDescriptor);
    standardPipeline.BindDescriptor(materialDescriptor);
    standardPipeline.BindDescriptor(objectDescriptor);

    mesh.RecordRender();

    device.EndRenderPass();

    swap.BlitToSwap(framebuffer.colorTexture());

    device.EndGraphicsCommandBuffer();

    device.SubmitGraphicsQueueAndPresent(timelineSemaphore, swap);

    timelineSemaphore.Wait();

    return MXC_SUCCESS;
}