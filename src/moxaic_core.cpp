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

#include "descriptors/moxaic_global_descriptor.hpp"
#include "descriptors/moxaic_material_descriptor.hpp"

#include "pipelines/moxaic_standard_pipeline.hpp"

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
    MXC_CHK(WindowInit());
    MXC_CHK(VulkanInit(g_pSDLWindow,
                       true));

    g_pDevice = new VulkanDevice();
    MXC_CHK(g_pDevice->Init());

    g_pFramebuffer = new VulkanFramebuffer(*g_pDevice);
    MXC_CHK(g_pFramebuffer->Init(g_WindowDimensions,
                                 Locality::Local));

    g_pCamera = new Camera();
    g_pCamera->transform().position() = {0, 0, -2};

    g_pGlobalDescriptor = new GlobalDescriptor(*g_pDevice);
    MXC_CHK(g_pGlobalDescriptor->Init(*g_pCamera, g_WindowDimensions));

    g_pTexture = new VulkanTexture(*g_pDevice);
    g_pTexture->Init(VK_FORMAT_R8G8B8A8_SRGB,
                     {128, 128},
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     Locality::Local);

    g_pMaterialDescriptor = new MaterialDescriptor(*g_pDevice);
    MXC_CHK(g_pMaterialDescriptor->Init(*g_pTexture));

    g_pTransform = new Transform();

    g_pObjectDescriptor = new ObjectDescriptor(*g_pDevice);
    MXC_CHK(g_pObjectDescriptor->Init(*g_pTransform));

    g_pStandardPipeline = new StandardPipeline(*g_pDevice);
    MXC_CHK(g_pStandardPipeline->Init(*g_pGlobalDescriptor, *g_pMaterialDescriptor, *g_pObjectDescriptor));

    g_pSwap = new VulkanSwap(*g_pDevice);
    MXC_CHK(g_pSwap->Init(g_WindowDimensions,
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
    while (g_ApplicationRunning) {
        WindowPoll();

        g_pDevice->BeginGraphicsCommandBuffer();
        g_pDevice->BeginRenderPass(*g_pFramebuffer);


        g_pDevice->EndRenderPass();

        g_pSwap->BlitToSwap(g_pFramebuffer->colorTexture().vkImage());

        g_pDevice->EndGraphicsCommandBuffer();

        g_pDevice->SubmitGraphicsQueueAndPresent(*g_pTimelineSemaphore, *g_pSwap);

        g_pTimelineSemaphore->Wait();
    }

    WindowShutdown();

    return MXC_SUCCESS;
}