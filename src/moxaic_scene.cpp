#include "moxaic_scene.hpp"
#include "moxaic_window.hpp"

#include <thread>

using namespace Moxaic;
using namespace glm;

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

    m_MainCamera.transform.position_ = vec3(0, 0, -1);
    m_MainCamera.transform.Rotate(0, 180, 0);
    m_MainCamera.UpdateView();
    m_MainCamera.UpdateProjection();

    m_SphereTestTransform.position_ = vec3(1, 0, 0);
    MXC_CHK(m_SphereTestTexture.InitFromFile("textures/test.jpg",
                                             Vulkan::Locality::Local));
    MXC_CHK(m_SphereTestTexture.TransitionInitialImmediate(Vulkan::PipelineType::Graphics));
    MXC_CHK(m_SphereTestMesh.InitSphere(0.5f));

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
    m_NodeReference.pExportedGlobalDescriptor()->localBuffer_ = m_GlobalDescriptor.GetLocalBuffer();
    m_NodeReference.pExportedGlobalDescriptor()->WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(const uint32_t& deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        m_GlobalDescriptor.SetLocalBufferView(m_MainCamera);
        m_GlobalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = device->BeginGraphicsCommandBuffer();

    device->ResetTimestamps();
    device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto* const nodeSemaphore = m_NodeReference.pExportedSemaphore();
    nodeSemaphore->SyncLocalWaitValue();
    if (nodeSemaphore->GetLocalWaitValue() != m_PriorNodeSemaphoreWaitValue) {
        m_PriorNodeSemaphoreWaitValue = nodeSemaphore->GetLocalWaitValue();
        m_NodeFramebufferIndex = !m_NodeFramebufferIndex;

        const auto& nodeFramebuffer = m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToGraphicsRead);

        auto& lastUsedNodeDescriptorBuffer = m_NodeReference.pExportedGlobalDescriptor()->localBuffer_;
        m_MeshNodeDescriptor[m_NodeFramebufferIndex].SetLocalBuffer(lastUsedNodeDescriptorBuffer);
        m_MeshNodeDescriptor[m_NodeFramebufferIndex].WriteLocalBuffer();

        m_NodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(m_MainCamera);
        m_NodeReference.pExportedGlobalDescriptor()->WriteLocalBuffer();
    }

    const auto& framebuffer = m_Framebuffers[m_FramebufferIndex];
    device->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.1f, 0.2f, 0.3f, 0.0f}});

    // m_StandardPipeline.BindGraphicsPipeline(commandBuffer);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_StandardMaterialDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_ObjectDescriptor);
    // m_SphereTestMesh.RecordRender(commandBuffer);

    m_MeshNodePipeline.BindPipeline(commandBuffer);
    m_MeshNodePipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    m_MeshNodePipeline.BindDescriptor(commandBuffer, m_MeshNodeDescriptor[m_NodeFramebufferIndex]);


    Vulkan::VkFunc.CmdDrawMeshTasksEXT(commandBuffer, 1, 1, 1);

    vkCmdEndRenderPass(commandBuffer);

    uint32_t swapIndex;
    m_Swap.Acquire(&swapIndex);
    m_Swap.BlitToSwap(commandBuffer,
                      swapIndex,
                      framebuffer.GetColorTexture());

    device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    device->SubmitGraphicsQueueAndPresent(m_Swap,
                                             swapIndex,
                                             &m_Semaphore);

    m_Semaphore.Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    const auto timestamps = device->GetTimestamps();
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

    m_MainCamera.transform.position_ = vec3(0, 0, -2);
    m_MainCamera.transform.Rotate(0, 180, 0);
    m_MainCamera.UpdateProjection();
    m_MainCamera.UpdateView();

    MXC_CHK(m_ComputeNodeProjectPipeline.Init("./shaders/compute_node.comp.spv"));
    MXC_CHK(m_ComputeNodePostPipeline.Init("./shaders/compute_node_post.comp.spv"));

    MXC_CHK(m_GlobalDescriptor.Init(m_MainCamera,
                                    Window::extents()));

    MXC_CHK(m_NodeReference.Init(pipelineType));

    MXC_CHK(m_OutputAtomicTexture.Init(VK_FORMAT_R32_UINT,
                                       Window::extents(),
                                       VK_IMAGE_USAGE_STORAGE_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                       Vulkan::Locality::Local));
    MXC_CHK(m_OutputAtomicTexture.TransitionInitialImmediate(Vulkan::PipelineType::Compute));

    MXC_CHK(m_ComputeNodeDescriptor.Init(m_GlobalDescriptor.GetLocalBuffer(),
                                     m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex),
                                     m_OutputAtomicTexture,
                                     m_Swap.GetVkSwapImageViews(m_NodeFramebufferIndex)));

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(m_NodeReference.ExportOverIPC(m_Semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m_NodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(m_MainCamera);
    m_NodeReference.pExportedGlobalDescriptor()->WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT ComputeCompositorScene::Loop(const uint32_t& deltaTime)
{
    if (m_MainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        m_GlobalDescriptor.SetLocalBufferView(m_MainCamera);
        m_GlobalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = device->BeginComputeCommandBuffer();

    device->ResetTimestamps();
    device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto* const nodeSemaphore = m_NodeReference.pExportedSemaphore();
    nodeSemaphore->SyncLocalWaitValue();
    if (nodeSemaphore->GetLocalWaitValue() != m_PriorNodeSemaphoreWaitValue) {
        m_PriorNodeSemaphoreWaitValue = nodeSemaphore->GetLocalWaitValue();
        m_NodeFramebufferIndex = !m_NodeFramebufferIndex;

        const auto& nodeFramebuffer = m_NodeReference.GetExportedFramebuffers(m_NodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToComputeRead);
        m_ComputeNodeDescriptor.WriteFramebuffer(nodeFramebuffer);

        auto& lastUsedNodeDescriptorBuffer = m_NodeReference.pExportedGlobalDescriptor()->localBuffer_;
        m_ComputeNodeDescriptor.SetLocalBuffer(lastUsedNodeDescriptorBuffer);
        m_ComputeNodeDescriptor.WriteLocalBuffer();

        m_NodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(m_MainCamera);
        m_NodeReference.pExportedGlobalDescriptor()->WriteLocalBuffer();

        MXC_LOG("CHILD UPDATE");
    }

    uint32_t swapIndex;
    m_Swap.Acquire(&swapIndex);
    m_Swap.Transition(commandBuffer,
                      swapIndex,
                      Vulkan::FromComputeSwapPresent,
                      Vulkan::ToComputeWrite);

    const auto& swap = m_Swap.GetVkSwapImages(swapIndex);
    const auto& swapView = m_Swap.GetVkSwapImageViews(swapIndex);
    m_ComputeNodeDescriptor.WriteOutputColorImage(swapView);
    m_ComputeNodeDescriptor.WriteOutputAtomicTexture(m_OutputAtomicTexture);

    Vulkan::ComputeNodePipeline::BindDescriptor(commandBuffer, m_GlobalDescriptor);
    Vulkan::ComputeNodePipeline::BindDescriptor(commandBuffer, m_ComputeNodeDescriptor);

    const auto groupCount = VkExtent2D(Window::extents().width / Vulkan::ComputeNodePipeline::LocalSize,
                                       Window::extents().height / Vulkan::ComputeNodePipeline::LocalSize);

    m_ComputeNodeProjectPipeline.BindPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);

    const VkImageMemoryBarrier atomicImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_OutputAtomicTexture.GetVkImage(),
        .subresourceRange = Vulkan::DefaultColorSubresourceRange,
      };
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &atomicImageMemoryBarrier);

    m_ComputeNodePostPipeline.BindPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);

    m_Swap.Transition(commandBuffer,
                      swapIndex,
                      Vulkan::FromComputeWrite,
                      Vulkan::ToComputeSwapPresent);

    device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    device->SubmitComputeQueueAndPresent(commandBuffer,
                                            m_Swap,
                                            swapIndex,
                                            &m_Semaphore);

    m_Semaphore.Wait();

    const auto timestamps = device->GetTimestamps();
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

    m_SpherTestTransform.position_ = vec3(0, 0, 0);
    MXC_CHK(m_SphereTestMesh.InitSphere(0.5f));
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

    MXC_CHK(m_Node.pImportedCompositorSemaphore()->SyncLocalWaitValue());
    MXC_CHK(m_Node.pImportedNodeSemaphore()->SyncLocalWaitValue());

    return MXC_SUCCESS;
}

int temp = 0;

MXC_RESULT NodeScene::Loop(const uint32_t& deltaTime)
{
    m_GlobalDescriptor.WriteBuffer(m_Node.pImportedGlobalDescriptor()->GetSharedBuffer());

    const auto commandBuffer = device->BeginGraphicsCommandBuffer();

    const auto& framebuffer = m_Node.framebuffer(m_FramebufferIndex);
    framebuffer.Transition(commandBuffer,
                           Vulkan::AcquireFromExternal,
                           Vulkan::ToGraphicsAttach);

    device->BeginRenderPass(framebuffer,
                               (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}});

    m_StandardPipeline.BindPipeline(commandBuffer);
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
    device->SubmitGraphicsQueue(m_Node.pImportedNodeSemaphore());
    // k_pDevice->SubmitGraphicsQueueAndPresent(m_Swap,
    //                                          swapIndex,
    //                                          m_Node.ImportedNodeSemaphore());

    m_Node.pImportedNodeSemaphore()->Wait();
    m_Node.pImportedCompositorSemaphore()->IncrementLocalWaitValue(m_CompositorSempahoreStep);
    m_Node.pImportedCompositorSemaphore()->Wait();

    m_FramebufferIndex = !m_FramebufferIndex;

    temp++;
    if (temp == 3) {
        _exit(1);
    }

    return MXC_SUCCESS;
}
