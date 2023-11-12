#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_swap.hpp"
#include "moxaic_vulkan_timeline_semaphore.hpp"
#include "moxaic_vulkan_mesh.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"

#include "moxaic_standard_pipeline.hpp"

using namespace Moxaic;

VulkanDevice *g_pDevice;
VulkanFramebuffer *g_pFramebuffer;
Camera *g_pCamera;
GlobalDescriptor *g_pGlobalDescriptor;
MaterialDescriptor *g_pMaterialDescriptor;
ObjectDescriptor *g_pObjectDescriptor;
VulkanSwap *g_pSwap;
VulkanTimelineSemaphore *g_pTimelineSemaphore;
VulkanMesh *g_pMesh;
VulkanTexture *g_pTexture;
StandardPipeline *g_pStandardPipeline;
Transform *g_pTransform;

MXC_RESULT Moxaic::CoreInit()
{
    MXC_CHK(Window::Init());
    MXC_CHK(VulkanInit(Moxaic::Window::window(),
                       true));

    g_pDevice = new VulkanDevice();
    MXC_CHK(g_pDevice->Init());

    g_pFramebuffer = new VulkanFramebuffer(*g_pDevice);
    MXC_CHK(g_pFramebuffer->Init(Window::extents(),
                                 Locality::Local));

    g_pCamera = new Camera();
    g_pCamera->transform().setPosition({0, 0, -2});
    g_pCamera->transform().Rotate(0, 180, 0);
    g_pCamera->UpdateView();

    g_pGlobalDescriptor = new GlobalDescriptor(*g_pDevice);
    MXC_CHK(g_pGlobalDescriptor->Init(*g_pCamera, Window::extents()));

    g_pTexture = new VulkanTexture(*g_pDevice);
    g_pTexture->InitFromFile("textures/test.jpg",
                             Locality::Local);
    g_pTexture->TransitionImmediateInitialToGraphicsRead();

    g_pMaterialDescriptor = new MaterialDescriptor(*g_pDevice);
    MXC_CHK(g_pMaterialDescriptor->Init(*g_pTexture));

    g_pTransform = new Transform();
    g_pTransform->setPosition({0, 0, 4});

    g_pObjectDescriptor = new ObjectDescriptor(*g_pDevice);
    MXC_CHK(g_pObjectDescriptor->Init(*g_pTransform));

    g_pStandardPipeline = new StandardPipeline(*g_pDevice);
    MXC_CHK(g_pStandardPipeline->Init(*g_pGlobalDescriptor, *g_pMaterialDescriptor, *g_pObjectDescriptor));

    g_pSwap = new VulkanSwap(*g_pDevice);
    MXC_CHK(g_pSwap->Init(Window::extents(),
                          false));

    g_pTimelineSemaphore = new VulkanTimelineSemaphore(*g_pDevice);
    MXC_CHK(g_pTimelineSemaphore->Init(false,
                                       Locality::Local));

    g_pMesh = new VulkanMesh(*g_pDevice);
    MXC_CHK(g_pMesh->Init());

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::CoreLoop()
{
    auto &device = *g_pDevice;
    auto &swap = *g_pSwap;
    auto &timelineSemaphore = *g_pTimelineSemaphore;
    auto &framebuffer = *g_pFramebuffer;
    auto &globalDescriptor = *g_pGlobalDescriptor;
    auto &standardPipeline = *g_pStandardPipeline;
    auto &mesh = *g_pMesh;
    auto &camera = *g_pCamera;

    Uint32 time = 0;
    Uint32 priorTime = 0;

    while (g_ApplicationRunning) {

        time = SDL_GetTicks();
        Uint32 deltaTime = time - priorTime;
        priorTime = time;

        Window::Poll();

        if (camera.Update(deltaTime)) {
            // should camera auto update descriptor somehow?
            globalDescriptor.UpdateView(*g_pCamera);
        }

        device.BeginGraphicsCommandBuffer();
        device.BeginRenderPass(*g_pFramebuffer);

        standardPipeline.BindPipeline();
        standardPipeline.BindDescriptor(*g_pGlobalDescriptor);
        standardPipeline.BindDescriptor(*g_pMaterialDescriptor);
        standardPipeline.BindDescriptor(*g_pObjectDescriptor);

        mesh.RecordRender();

        device.EndRenderPass();

        swap.BlitToSwap(framebuffer.colorTexture());

        device.EndGraphicsCommandBuffer();

        device.SubmitGraphicsQueueAndPresent(timelineSemaphore, swap);

        timelineSemaphore.Wait();
    }

    Window::Shutdown();

    return MXC_SUCCESS;
}