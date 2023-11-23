#include "moxaic_scene.hpp"
#include "moxaic_window.hpp"

#include <thread>

using namespace Moxaic;

MXC_RESULT CompositorScene::Init()
{
    for (int i = 0; i < m_Framebuffers.size(); ++i) {
        MXC_CHK(m_Framebuffers[i].Init(Window::extents(),
                                       Vulkan::Locality::Local));
    }

    m_MainCamera.Transform()->SetPosition(glm::vec3(0, 0, -2));
    m_MainCamera.Transform()->Rotate(0, 180, 0);
    m_MainCamera.SetAspect(Window::extents().width / Window::extents().height);
    m_MainCamera.UpdateView();
    m_MainCamera.UpdateProjection();

    m_SphereTestTransform.SetPosition(glm::vec3(1, 0, 0));
    MXC_CHK2(m_SphereTestTexture.InitFromFile("textures/test.jpg",
                                              Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_SphereTestMesh.InitSphere());

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera,
                                    Window::extents()));
    MXC_CHK(m_StandardMaterialDescriptor.Init(m_SphereTestTexture));
    MXC_CHK(m_ObjectDescriptor.Init(m_SphereTestTransform));
    MXC_CHK(m_StandardPipeline.Init(m_GlobalDescriptor,
                                    m_StandardMaterialDescriptor,
                                    m_ObjectDescriptor));

    MXC_CHK(m_Swap.Init(Window::extents(),
                        false));

    MXC_CHK(m_Semaphore.Init(true,
                             Vulkan::Locality::External));

    MXC_CHK(m_NodeReference.Init());

    for (int i = 0; i < m_MeshNodeDescriptor.size(); ++i) {
        MXC_CHK(m_MeshNodeDescriptor[i].Init(
          m_GlobalDescriptor.GetLocalBuffer(),
          m_NodeReference.ExportedFramebuffers(i)));
    }
    MXC_CHK(m_MeshNodePipeline.Init(m_GlobalDescriptor,
                                    m_MeshNodeDescriptor[0]));

    //    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(m_NodeReference.ExportOverIPC(m_Semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m_NodeReference.ExportedGlobalDescriptor()->SetLocalBuffer(m_GlobalDescriptor.GetLocalBuffer());
    m_NodeReference.ExportedGlobalDescriptor()->PushLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(uint32_t const& deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        auto const localBuffer = m_GlobalDescriptor.LocalBuffer();
        localBuffer->view = m_MainCamera.GetView();
        localBuffer->invView = m_MainCamera.GetInverseView();
        m_GlobalDescriptor.PushLocalBuffer();
    }

    k_pDevice->BeginGraphicsCommandBuffer();

    // auto const nodeSemaphore = m_NodeReference.Semaphore();
    // auto* const nodeSemaphore = m_NodeReference.Semaphore();
    auto* const nodeSemaphore = m_NodeReference.ExportedSemaphore();
    nodeSemaphore->SyncLocalWaitValue();
    if (nodeSemaphore->GetLocalWaitValue() != m_PriorNodeSemaphoreWaitValue) {
        m_PriorNodeSemaphoreWaitValue = nodeSemaphore->GetLocalWaitValue();
        m_NodeFramebufferIndex = !m_NodeFramebufferIndex;

        auto const& nodeFramebuffer = m_NodeReference.ExportedFramebuffers(m_NodeFramebufferIndex);
        nodeFramebuffer.Transition(Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToGraphicsRead);

        auto& lastUsedNodeDescriptorBuffer = m_NodeReference.ExportedGlobalDescriptor()->GetLocalBuffer();
        m_MeshNodeDescriptor[m_NodeFramebufferIndex].Update(lastUsedNodeDescriptorBuffer);

        // Condense nearz/farz to draw radius of node
        auto const viewPosition = m_MainCamera.GetView() * glm::vec4(m_NodeReference.Transform()->GetPosition(), 1);
        float const viewDistanceToCenter = -viewPosition.z;
        float const offset = m_NodeReference.GetDrawRadius() * 0.5f;
        float const farZ = viewDistanceToCenter + offset;
        float nearZ = viewDistanceToCenter - offset;
        if (nearZ < m_MainCamera.GetNear()) {
            nearZ = m_MainCamera.GetNear();
        }
        auto const localBuffer = m_NodeReference.ExportedGlobalDescriptor()->LocalBuffer();
        *localBuffer = m_GlobalDescriptor.GetLocalBuffer();
        localBuffer->proj = glm::perspective(m_MainCamera.GetFOV(),
                                             m_MainCamera.GetAspect(),
                                             nearZ,
                                             farZ);
        localBuffer->invProj = glm::inverse(localBuffer->proj);
        m_NodeReference.ExportedGlobalDescriptor()->PushLocalBuffer();
    }

    auto const& framebuffer = m_Framebuffers[m_FramebufferIndex];
    k_pDevice->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.1f, 0.2f, 0.3f, 0.0f}});

    m_StandardPipeline.BindPipeline();
    m_StandardPipeline.BindDescriptor(m_GlobalDescriptor);
    m_StandardPipeline.BindDescriptor(m_StandardMaterialDescriptor);
    m_StandardPipeline.BindDescriptor(m_ObjectDescriptor);
    m_SphereTestMesh.RecordRender();

    m_MeshNodePipeline.BindPipeline();
    m_MeshNodePipeline.BindDescriptor(m_GlobalDescriptor);
    m_MeshNodePipeline.BindDescriptor(m_MeshNodeDescriptor[m_NodeFramebufferIndex]);
    Vulkan::VkFunc.CmdDrawMeshTasksEXT(k_pDevice->GetVkGraphicsCommandBuffer(),
                                       1,
                                       1,
                                       1);

    k_pDevice->EndRenderPass();

    m_Swap.Acquire();
    m_Swap.BlitToSwap(framebuffer.colorTexture());

    k_pDevice->EndGraphicsCommandBuffer();

    k_pDevice->SubmitGraphicsQueueAndPresent(m_Semaphore, m_Swap);

    m_Semaphore.Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Init()
{
    MXC_CHK(m_Node.Init());

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera,
                                    Window::extents()));

    m_SpherTestTransform.SetPosition(glm::vec3(0, 0, 0));
    MXC_CHK(m_SphereTestTexture.InitFromFile("textures/uvgrid.jpg",
                                             Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_MaterialDescriptor.Init(m_SphereTestTexture));
    MXC_CHK(m_ObjectDescriptor.Init(m_SpherTestTransform));
    MXC_CHK(m_StandardPipeline.Init(m_GlobalDescriptor,
                                    m_MaterialDescriptor,
                                    m_ObjectDescriptor));

    MXC_CHK(m_SphereTestMesh.InitSphere());

    MXC_CHK(m_Swap.Init(Window::extents(),
                        false));

    // spin lock for now until initial ipc message is received
    while (m_Node.ipcFromCompositor().Deque() == 0) {
        MXC_LOG("Node Waiting for IPC");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MXC_CHK(m_Node.CompositorSemaphore().SyncLocalWaitValue());
    MXC_CHK(m_Node.NodeSemaphore().SyncLocalWaitValue());

    return MXC_SUCCESS;
}

int temp = 0;

MXC_RESULT NodeScene::Loop(uint32_t const& deltaTime)
{
    m_GlobalDescriptor.PushBuffer(m_Node.ImportedGlobalDescriptor()->GetSharedBuffer());

    k_pDevice->BeginGraphicsCommandBuffer();

    auto const& framebuffer = m_Node.framebuffer(m_FramebufferIndex);
    framebuffer.Transition(Vulkan::AcquireFromExternal, Vulkan::ToGraphicsAttach);

    k_pDevice->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}});

    m_StandardPipeline.BindPipeline();
    m_StandardPipeline.BindDescriptor(m_GlobalDescriptor);
    m_StandardPipeline.BindDescriptor(m_MaterialDescriptor);
    m_StandardPipeline.BindDescriptor(m_ObjectDescriptor);

    m_SphereTestMesh.RecordRender();

    k_pDevice->EndRenderPass();

    framebuffer.Transition(Vulkan::FromGraphicsAttach,
                           Vulkan::ReleaseToExternalGraphicsRead);

    // m_Swap.Acquire();
    // m_Swap.BlitToSwap(m_Node.framebuffer(m_FramebufferIndex).colorTexture());

    k_pDevice->EndGraphicsCommandBuffer();

    k_pDevice->SubmitGraphicsQueue(&m_Node.NodeSemaphore());
    // k_pDevice->SubmitGraphicsQueueAndPresent(m_Node.NodeSemaphore(), m_Swap);

    m_Node.NodeSemaphore().Wait();
    m_Node.CompositorSemaphore().IncrementWaitValue(m_CompositorSempahoreStep);
    m_Node.CompositorSemaphore().Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    // temp++;
    // if (temp == 10) {
    //     _exit(1);
    // }

    return MXC_SUCCESS;
}
