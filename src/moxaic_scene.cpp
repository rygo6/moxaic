#include "moxaic_scene.hpp"

#include <thread>
#include <chrono>

using namespace Moxaic;

MXC_RESULT CompositorScene::Init()
{
    for (int i = 0; i < m_Framebuffers.size(); ++i) {
        MXC_CHK(m_Framebuffers[i].Init(Window::extents(),
                                       Vulkan::Locality::Local));
    }

    m_MainCamera.transform().setPosition({0, 0, -2});
    m_MainCamera.transform().Rotate(0, 180, 0);
    m_MainCamera.UpdateView();

    m_SphereTestTransform.setPosition({2, 0, 0});

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera,
                                    Window::extents()));

    MXC_CHK(m_SphereTestTexture.InitFromFile("textures/test.jpg",
                                             Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionImmediateInitialToGraphicsRead());

    MXC_CHK(m_MaterialDescriptor.Init(m_SphereTestTexture));

    MXC_CHK(m_ObjectDescriptor.Init(m_SphereTestTransform));

    MXC_CHK(m_StandardPipeline.Init(m_GlobalDescriptor,
                                    m_MaterialDescriptor,
                                    m_ObjectDescriptor));

    MXC_CHK(m_Swap.Init(Window::extents(),
                        false));

    MXC_CHK(m_Semaphore.Init(true,
                             Vulkan::Locality::External));

    MXC_CHK(m_SphereTestMesh.Init());

    MXC_CHK(m_CompositorNode.Init());

//    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(m_CompositorNode.ExportOverIPC(m_Semaphore));

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(const uint32_t deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        m_GlobalDescriptor.UpdateView(m_MainCamera);
    }

    k_Device.BeginGraphicsCommandBuffer();
    k_Device.BeginRenderPass(m_Framebuffers[m_FramebufferIndex]);

    m_StandardPipeline.BindPipeline();
    m_StandardPipeline.BindDescriptor(m_GlobalDescriptor);
    m_StandardPipeline.BindDescriptor(m_MaterialDescriptor);
    m_StandardPipeline.BindDescriptor(m_ObjectDescriptor);

    m_SphereTestMesh.RecordRender();

    k_Device.EndRenderPass();

    m_Swap.Acquire();
    m_Swap.BlitToSwap(m_Framebuffers[m_FramebufferIndex].colorTexture());

    k_Device.EndGraphicsCommandBuffer();

    k_Device.SubmitGraphicsQueueAndPresent(m_Semaphore, m_Swap);

    m_Semaphore.Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

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

    // spin lock for now until initial ipc message is received
    while (node.ipcFromCompositor().Deque() == 0) {
        MXC_LOG("Node Waiting for IPC");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Loop(const uint32_t deltaTime)
{
    return MXC_SUCCESS;
}
