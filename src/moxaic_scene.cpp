#include "moxaic_scene.hpp"
#include "moxaic_window.hpp"

#include <thread>

using namespace Moxaic;

MXC_RESULT CompositorScene::Init()
{
    const auto pipelineType = Vulkan::PipelineType::Graphics;
    for (int i = 0; i < m_Framebuffers.size(); ++i) {
        MXC_CHK(m_Framebuffers[i].Init(pipelineType,
                                       Window::extents(),
                                       Vulkan::Locality::Local));
    }

    MXC_CHK(m_Swap.Init(pipelineType,
                        Window::extents()));
    MXC_CHK(m_Semaphore.Init(true, Vulkan::Locality::External));

    m_MainCamera.Transform()->SetPosition(glm::vec3(0, 0, -2));
    m_MainCamera.Transform()->Rotate(0, 180, 0);
    m_MainCamera.UpdateView();
    m_MainCamera.UpdateProjection();

    m_SphereTestTransform.SetPosition(glm::vec3(1, 0, 0));
    MXC_CHK(m_SphereTestTexture.InitFromFile("textures/test.jpg",
                                             Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionInitialImmediate(Vulkan::PipelineType::Graphics));
    MXC_CHK(m_SphereTestMesh.InitSphere());

    MXC_CHK(m_StandardPipeline.Init());
    MXC_CHK(m_MeshNodePipeline.Init());

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera, Window::extents()));
    MXC_CHK(m_StandardMaterialDescriptor.Init(m_SphereTestTexture));
    MXC_CHK(m_ObjectDescriptor.Init(m_SphereTestTransform));

    MXC_CHK(m_NodeReference.Init(Vulkan::PipelineType::Graphics));

    for (int i = 0; i < m_MeshNodeDescriptor.size(); ++i) {
        MXC_CHK(m_MeshNodeDescriptor[i].Init(m_GlobalDescriptor.GetLocalBuffer(),
                                             m_NodeReference.GetExportedFramebuffers(i)));
    }

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(m_NodeReference.ExportOverIPC(m_Semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m_NodeReference.ExportedGlobalDescriptor()->SetLocalBuffer(m_GlobalDescriptor.GetLocalBuffer());
    m_NodeReference.ExportedGlobalDescriptor()->WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(const uint32_t& deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        m_GlobalDescriptor.SetLocalBufferView(m_MainCamera);
        m_GlobalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = k_pDevice->BeginGraphicsCommandBuffer();

    k_pDevice->ResetTimestamps();
    k_pDevice->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto* const nodeSemaphore = m_NodeReference.ExportedSemaphore();
    nodeSemaphore->SyncLocalWaitValue();
    if (nodeSemaphore->GetLocalWaitValue() != m_PriorNodeSemaphoreWaitValue) {
        m_PriorNodeSemaphoreWaitValue = nodeSemaphore->GetLocalWaitValue();
        m_NodeFramebufferIndex = !m_NodeFramebufferIndex;

        const auto& nodeFramebuffer = m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToGraphicsRead);

        auto& lastUsedNodeDescriptorBuffer = m_NodeReference.ExportedGlobalDescriptor()->GetLocalBuffer();
        m_MeshNodeDescriptor[m_NodeFramebufferIndex].SetLocalBuffer(lastUsedNodeDescriptorBuffer);
        m_MeshNodeDescriptor[m_NodeFramebufferIndex].WriteLocalBuffer();

        m_NodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(m_MainCamera);
        m_NodeReference.ExportedGlobalDescriptor()->WriteLocalBuffer();
    }

    const auto& framebuffer = m_Framebuffers[m_FramebufferIndex];
    k_pDevice->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.1f, 0.2f, 0.3f, 0.0f}});

    // m_StandardPipeline.BindGraphicsPipeline(commandBuffer);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_StandardMaterialDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_ObjectDescriptor);
    // m_SphereTestMesh.RecordRender(commandBuffer);

    m_MeshNodePipeline.BindGraphicsPipeline(commandBuffer);
    m_MeshNodePipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    m_MeshNodePipeline.BindDescriptor(commandBuffer, m_MeshNodeDescriptor[m_NodeFramebufferIndex]);


    Vulkan::VkFunc.CmdDrawMeshTasksEXT(commandBuffer, 1, 1, 1);

    vkCmdEndRenderPass(commandBuffer);

    uint32_t swapIndex;
    m_Swap.Acquire(&swapIndex);
    m_Swap.BlitToSwap(commandBuffer,
                      swapIndex,
                      framebuffer.GetColorTexture());

    k_pDevice->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    k_pDevice->SubmitGraphicsQueueAndPresent(m_Swap,
                                             swapIndex,
                                             &m_Semaphore);

    m_Semaphore.Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    const auto timestamps = k_pDevice->GetTimestamps();
    const float taskMeshMs = timestamps[1] - timestamps[0];
    MXC_LOG_NAMED(taskMeshMs);

    return MXC_SUCCESS;
}

MXC_RESULT ComputeCompositorScene::Init()
{
    const auto pipelineType = Vulkan::PipelineType::Compute;
    MXC_CHK(m_Swap.Init(pipelineType,
                        Window::extents()));
    MXC_CHK(m_Semaphore.Init(true, Vulkan::Locality::External));

    m_MainCamera.Transform()->SetPosition(glm::vec3(0, 0, -2));
    m_MainCamera.Transform()->Rotate(0, 180, 0);
    m_MainCamera.UpdateView();
    m_MainCamera.UpdateProjection();

    MXC_CHK(m_ComputeNodePipeline.Init());

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera, Window::extents()));

    MXC_CHK(m_NodeReference.Init(pipelineType));

    MXC_CHK(m_ComputeNodeDescriptor.Init(m_GlobalDescriptor.GetLocalBuffer(),
                                         m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex),
                                         m_Swap.GetVkSwapImageViews(m_NodeFramebufferIndex)));

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(m_NodeReference.ExportOverIPC(m_Semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m_NodeReference.ExportedGlobalDescriptor()->SetLocalBuffer(m_GlobalDescriptor.GetLocalBuffer());
    m_NodeReference.ExportedGlobalDescriptor()->WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT ComputeCompositorScene::Loop(const uint32_t& deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        m_GlobalDescriptor.SetLocalBufferView(m_MainCamera);
        m_GlobalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = k_pDevice->BeginComputeCommandBuffer();

    k_pDevice->ResetTimestamps();
    k_pDevice->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto* const nodeSemaphore = m_NodeReference.ExportedSemaphore();
    nodeSemaphore->SyncLocalWaitValue();
    if (nodeSemaphore->GetLocalWaitValue() != m_PriorNodeSemaphoreWaitValue) {
        m_PriorNodeSemaphoreWaitValue = nodeSemaphore->GetLocalWaitValue();
        m_NodeFramebufferIndex = !m_NodeFramebufferIndex;

        const auto& nodeFramebuffer = m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToComputeRead);
        m_ComputeNodeDescriptor.WriteFramebuffer(nodeFramebuffer);

        auto& lastUsedNodeDescriptorBuffer = m_NodeReference.ExportedGlobalDescriptor()->GetLocalBuffer();
        m_ComputeNodeDescriptor.SetLocalBuffer(lastUsedNodeDescriptorBuffer);
        m_ComputeNodeDescriptor.WriteLocalBuffer();

        m_NodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(m_MainCamera);
        m_NodeReference.ExportedGlobalDescriptor()->WriteLocalBuffer();
    }

    uint32_t swapIndex;
    m_Swap.Acquire(&swapIndex);

    m_Swap.Transition(commandBuffer,
                      swapIndex,
                      Vulkan::FromComputeSwapPresent,
                      Vulkan::ToComputeWrite);

    const auto& swap = m_Swap.GetVkSwapImages(swapIndex);
    const auto& swapView = m_Swap.GetVkSwapImageViews(swapIndex);
    m_ComputeNodeDescriptor.WriteOutputColorImage(m_Swap.GetVkSwapImageViews(swapIndex));

    m_ComputeNodePipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    m_ComputeNodePipeline.BindDescriptor(commandBuffer, m_ComputeNodeDescriptor);

    const auto groupCount = VkExtent2D(Window::extents().width / Vulkan::ComputeNodePipeline::LocalSize,
                                       Window::extents().height / Vulkan::ComputeNodePipeline::LocalSize);

    const VkImageMemoryBarrier imageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = swap,
      .subresourceRange = Vulkan::DefaultColorSubresourceRange,
    };

    m_ComputeNodePipeline.BindComputeClearPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);

    m_ComputeNodePipeline.BindComputePipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);

    m_Swap.Transition(commandBuffer,
                      swapIndex,
                      Vulkan::FromComputeWrite,
                      Vulkan::ToComputeSwapPresent);

    k_pDevice->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    k_pDevice->SubmitComputeQueueAndPresent(commandBuffer,
                                            m_Swap,
                                            swapIndex,
                                            &m_Semaphore);

    m_Semaphore.Wait();

    const auto timestamps = k_pDevice->GetTimestamps();
    const float computeMs = timestamps[1] - timestamps[0];
    MXC_LOG_NAMED(computeMs);

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Init()
{
    MXC_CHK(m_Node.Init());

    // MXC_CHK(m_Swap.Init(Window::extents(), Vulkan::CompositorPipelineType));

    MXC_CHK(m_StandardPipeline.Init());
    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera, Window::extents()));

    m_SpherTestTransform.SetPosition(glm::vec3(0, 0, 0));
    MXC_CHK(m_SphereTestMesh.InitSphere());
    MXC_CHK(m_SphereTestTexture.InitFromFile("textures/uvgrid.jpg",
                                             Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionInitialImmediate(Vulkan::PipelineType::Graphics));

    MXC_CHK(m_MaterialDescriptor.Init(m_SphereTestTexture));
    MXC_CHK(m_ObjectDescriptor.Init(m_SpherTestTransform));

    // spin lock for now until initial ipc message is received
    while (m_Node.ipcFromCompositor().Deque() == 0) {
        MXC_LOG("Node Waiting for IPC");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MXC_CHK(m_Node.ImportedCompositorSemaphore()->SyncLocalWaitValue());
    MXC_CHK(m_Node.ImportedNodeSemaphore()->SyncLocalWaitValue());

    return MXC_SUCCESS;
}

int temp = 0;

MXC_RESULT NodeScene::Loop(const uint32_t& deltaTime)
{
    m_GlobalDescriptor.WriteBuffer(m_Node.ImportedGlobalDescriptor()->GetSharedBuffer());

    const auto commandBuffer = k_pDevice->BeginGraphicsCommandBuffer();

    const auto& framebuffer = m_Node.framebuffer(m_FramebufferIndex);
    framebuffer.Transition(commandBuffer,
                           Vulkan::AcquireFromExternal,
                           Vulkan::ToGraphicsAttach);

    k_pDevice->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}});

    m_StandardPipeline.BindGraphicsPipeline(commandBuffer);
    m_StandardPipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    m_StandardPipeline.BindDescriptor(commandBuffer, m_MaterialDescriptor);
    m_StandardPipeline.BindDescriptor(commandBuffer, m_ObjectDescriptor);

    m_SphereTestMesh.RecordRender(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    // framebuffer.GetDepthTexture().BlitTo(commandBuffer,
    //                                      framebuffer.GetGBufferTexture());
    framebuffer.Transition(commandBuffer,
                           Vulkan::FromGraphicsAttach,
                           Vulkan::CompositorPipelineType == Vulkan::PipelineType::Graphics ?
                             Vulkan::ReleaseToExternalGraphicsRead :
                             Vulkan::ReleaseToExternalComputesRead);

    // uint32_t swapIndex;
    // m_Swap.Acquire(&swapIndex);
    // m_Swap.BlitToSwap(commandBuffer,
    //                   swapIndex,
    //                   m_Node.framebuffer(m_FramebufferIndex).colorTexture());

    vkEndCommandBuffer(commandBuffer);
    k_pDevice->SubmitGraphicsQueue(m_Node.ImportedNodeSemaphore());
    // k_pDevice->SubmitGraphicsQueueAndPresent(m_Swap,
    //                                          swapIndex,
    //                                          m_Node.ImportedNodeSemaphore());

    m_Node.ImportedNodeSemaphore()->Wait();
    m_Node.ImportedCompositorSemaphore()->IncrementLocalWaitValue(m_CompositorSempahoreStep);
    m_Node.ImportedCompositorSemaphore()->Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    temp++;
    if (temp == 5) {
        _exit(1);
    }

    return MXC_SUCCESS;
}
