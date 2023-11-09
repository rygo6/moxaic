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

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

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

bool g_CameraLocked = false;

enum MoveDirection
{
    Up = 1 << 0,
    Down = 1 << 1,
    Left = 1 << 2,
    Right = 1 << 3,
};

MoveDirection g_CameraMove;

static void UpdateCamera(const MouseMotionEvent &event)
{
    if (g_CameraLocked) {
        g_pCamera->transform().Rotate(0, -event.delta.x, 0);
        g_pCamera->UpdateView();
        g_pGlobalDescriptor->UpdateView(*g_pCamera);
    }
}

static void LockMouse(const MouseEvent &event)
{
    if (event.button == Button::Left && event.phase == Phase::Pressed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        g_CameraLocked = true;
    } else if (event.button == Button::Left && event.phase == Phase::Released) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        g_CameraLocked = false;
    }
}

static void WorldMove(const KeyEvent &event)
{
    switch (event.key) {
        case SDLK_w:
            g_CameraMove = (MoveDirection) (event.phase == Phase::Pressed ?
                                            g_CameraMove | MoveDirection::Up :
                                            g_CameraMove & ~MoveDirection::Up);
            break;
        case SDLK_s:
            g_CameraMove = (MoveDirection) (event.phase == Phase::Pressed ?
                                            g_CameraMove | MoveDirection::Down :
                                            g_CameraMove & ~MoveDirection::Down);
            break;
        case SDLK_a:
            g_CameraMove = (MoveDirection) (event.phase == Phase::Pressed ?
                                            g_CameraMove | MoveDirection::Left :
                                            g_CameraMove & ~MoveDirection::Left);
            break;
        case SDLK_d:
            g_CameraMove = (MoveDirection) (event.phase == Phase::Pressed ?
                                            g_CameraMove | MoveDirection::Right :
                                            g_CameraMove & ~MoveDirection::Right);
            break;
    }
}

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
    g_pCamera->transform().setPosition({0, 0, -2});
    g_pCamera->transform().Rotate(0, 180, 0);
    g_pCamera->UpdateView();

    g_pGlobalDescriptor = new GlobalDescriptor(*g_pDevice);
    MXC_CHK(g_pGlobalDescriptor->Init(*g_pCamera, g_WindowDimensions));

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
    MXC_CHK(g_pSwap->Init(g_WindowDimensions,
                          false));

    g_pTimelineSemaphore = new VulkanTimelineSemaphore(*g_pDevice);
    MXC_CHK(g_pTimelineSemaphore->Init(false,
                                       Locality::Local));

    g_pMesh = new VulkanMesh(*g_pDevice);
    MXC_CHK(g_pMesh->Init());

    g_MouseMotionSubscribers.push_back(UpdateCamera);
    g_MouseSubscribers.push_back(LockMouse);
    g_KeySubscribers.push_back(WorldMove);

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

        WindowPoll();

        if (g_CameraMove != 0) {
            auto delta = glm::vec3{0, 0, 0};
            if ((g_CameraMove & MoveDirection::Up) == MoveDirection::Up) {
                delta.z -= 1;
            }
            if ((g_CameraMove & MoveDirection::Down) == MoveDirection::Down) {
                delta.z += 1;
            }
            if ((g_CameraMove & MoveDirection::Left) == MoveDirection::Left) {
                delta.x -= 1;
            }
            if ((g_CameraMove & MoveDirection::Right) == MoveDirection::Right) {
                delta.x += 1;
            }
            camera.transform().LocalTranslate(delta * (float) deltaTime * 0.01f);
            camera.UpdateView();
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

    WindowShutdown();

    return MXC_SUCCESS;
}