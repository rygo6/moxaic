#include "moxaic_scene.hpp"
#include "moxaic_window.hpp"

#include <thread>

using namespace Moxaic;
using namespace glm;

MXC_RESULT CompositorScene::Init()
{
    const auto pipelineType = Vulkan::PipelineType::Graphics;
    for (int i = 0; i < framebuffers.size(); ++i) {
        MXC_CHK(framebuffers[i].Init(pipelineType,
                                     Window::GetExtents(),
                                     Vulkan::Locality::Local));
    }

    MXC_CHK(swap.Init(pipelineType,
                      Window::GetExtents()));
    MXC_CHK(semaphore.Init(true, Vulkan::Locality::External));

    mainCamera.transform.position_ = vec3(0, 0, -1);
    mainCamera.transform.Rotate(0, 180, 0);
    mainCamera.UpdateViewProjection();

    sphereTestTransform.position_ = vec3(1, 0, 0);
    MXC_CHK(sphereTestTexture.InitFromFile("textures/test.jpg",
                                           Vulkan::Locality::Local));
    MXC_CHK(sphereTestTexture.TransitionInitialImmediate(Vulkan::PipelineType::Graphics));
    MXC_CHK(sphereTestMesh.InitSphere(0.5f));

    MXC_CHK(standardPipeline.Init());
    MXC_CHK(meshNodePipeline.Init());

    MXC_CHK(globalDescriptor.Init(mainCamera, Window::GetExtents()));
    MXC_CHK(standardMaterialDescriptor.Init(sphereTestTexture));
    MXC_CHK(objectDescriptor.Init(sphereTestTransform));

    MXC_CHK(nodeReference.Init(Vulkan::PipelineType::Graphics));

    for (int i = 0; i < meshNodeDescriptor.size(); ++i) {
        MXC_CHK(meshNodeDescriptor[i].Init(globalDescriptor.localBuffer,
                                           nodeReference.GetExportedFramebuffer(i)));
    }

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MXC_CHK(nodeReference.ExportOverIPC(semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    nodeReference.exportedGlobalDescriptor.localBuffer = globalDescriptor.localBuffer;
    nodeReference.exportedGlobalDescriptor.WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT CompositorScene::Loop(const uint32_t& deltaTime)
{
    if (mainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        globalDescriptor.SetLocalBufferView(mainCamera);
        globalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = Device->BeginGraphicsCommandBuffer();

    Device->ResetTimestamps();
    Device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto& nodeSemaphore = nodeReference.exportedSemaphore;
    nodeSemaphore.SyncLocalWaitValue();
    if (nodeSemaphore.localWaitValue != priorNodeSemaphoreWaitValue) {
        priorNodeSemaphoreWaitValue = nodeSemaphore.localWaitValue;
        nodeFramebufferIndex = !nodeFramebufferIndex;

        const auto& nodeFramebuffer = nodeReference.GetExportedFramebuffer(nodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToGraphicsRead);

        const auto& lastUsedNodeDescriptorBuffer = nodeReference.exportedGlobalDescriptor.localBuffer;
        meshNodeDescriptor[nodeFramebufferIndex].SetLocalBuffer(lastUsedNodeDescriptorBuffer);
        meshNodeDescriptor[nodeFramebufferIndex].WriteLocalBuffer();

        nodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(mainCamera);
        nodeReference.exportedGlobalDescriptor.WriteLocalBuffer();
    }

    const auto& framebuffer = framebuffers[framebufferIndex];
    Device->BeginRenderPass(framebuffer,
                            (VkClearColorValue){{0.1f, 0.2f, 0.3f, 0.0f}});

    // m_StandardPipeline.BindGraphicsPipeline(commandBuffer);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_GlobalDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_StandardMaterialDescriptor);
    // m_StandardPipeline.BindDescriptor(commandBuffer, m_ObjectDescriptor);
    // m_SphereTestMesh.RecordRender(commandBuffer);

    meshNodePipeline.BindPipeline(commandBuffer);
    meshNodePipeline.BindDescriptor(commandBuffer, globalDescriptor);
    meshNodePipeline.BindDescriptor(commandBuffer, meshNodeDescriptor[nodeFramebufferIndex]);


    Vulkan::VkFunc.CmdDrawMeshTasksEXT(commandBuffer, 1, 1, 1);

    vkCmdEndRenderPass(commandBuffer);

    uint32_t swapIndex;
    swap.Acquire(&swapIndex);
    swap.BlitToSwap(commandBuffer,
                    swapIndex,
                    framebuffer.GetColorTexture());

    Device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    Device->SubmitGraphicsQueueAndPresent(swap,
                                          swapIndex,
                                          &semaphore);

    semaphore.Wait();

    framebufferIndex = !framebufferIndex;

    const auto timestamps = Device->GetTimestamps();
    const float taskMeshMs = timestamps[1] - timestamps[0];
    MXC_LOG_NAMED(taskMeshMs);

    return MXC_SUCCESS;
}

MXC_RESULT ComputeCompositorScene::Init()
{
    const auto pipelineType = Vulkan::PipelineType::Compute;
    MXC_CHK(swap.Init(pipelineType,
                      Window::GetExtents()));
    MXC_CHK(semaphore.Init(true, Vulkan::Locality::External));

    mainCamera.transform.position_ = vec3(0, 0, -2);
    mainCamera.transform.Rotate(0, 180, 0);
    mainCamera.UpdateViewProjection();

    MXC_CHK(computeNodePrePipeline.Init("./shaders/compute_node_pre.comp.spv"));
    MXC_CHK(computeNodePipeline.Init("./shaders/compute_node.comp.spv"));
    MXC_CHK(computeNodePostPipeline.Init("./shaders/compute_node_post.comp.spv"));

    MXC_CHK(globalDescriptor.Init(mainCamera,
                                  Window::GetExtents()));

    MXC_CHK(nodeReference.Init(pipelineType));

    MXC_CHK(outputAtomicTexture.Init(VK_FORMAT_R32_UINT,
                                     Window::GetExtents(),
                                     VK_IMAGE_USAGE_STORAGE_BIT,
                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                     Vulkan::Locality::Local));
    MXC_CHK(outputAtomicTexture.TransitionInitialImmediate(Vulkan::PipelineType::Compute));
    const auto averagedExtents = VkExtent2D(Window::GetExtents().width / Vulkan::ComputeNodePipeline::LocalSize,
                                            Window::GetExtents().height / Vulkan::ComputeNodePipeline::LocalSize);
    MXC_CHK(outputAveragedAtomicTexture.Init(VK_FORMAT_R32_UINT,
                                             averagedExtents,
                                             VK_IMAGE_USAGE_STORAGE_BIT,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             Vulkan::Locality::Local,
                                             VK_SAMPLE_COUNT_1_BIT));
    MXC_CHK(outputAveragedAtomicTexture.TransitionInitialImmediate(Vulkan::PipelineType::Compute));

    const Vulkan::ComputeNodeDescriptor::UniformBuffer computeNodeBuffer{
      .view = globalDescriptor.localBuffer.view,
      .proj = globalDescriptor.localBuffer.proj,
      .viewProj = globalDescriptor.localBuffer.viewProj,
      .invView = globalDescriptor.localBuffer.invView,
      .invProj = globalDescriptor.localBuffer.invProj,
      .invViewProj = globalDescriptor.localBuffer.invViewProj,
      .screenSize = globalDescriptor.localBuffer.screenSize,
    };
    MXC_CHK(computeNodeDescriptor.Init(computeNodeBuffer,
                                       nodeReference.GetExportedFramebuffer(nodeFramebufferIndex),
                                       outputAveragedAtomicTexture,
                                       outputAtomicTexture,
                                       swap.GetVkSwapImageView(nodeFramebufferIndex)));

    // why must I wait before exporting over IPC? Should it just fill in the memory and the other grab it when it can?
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    MXC_CHK(nodeReference.ExportOverIPC(semaphore));

    // and must wait again after node is inited on other side... wHyy!?!
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    nodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(mainCamera);
    nodeReference.exportedGlobalDescriptor.WriteLocalBuffer();

    return MXC_SUCCESS;
}

MXC_RESULT ComputeCompositorScene::Loop(const uint32_t& deltaTime)
{
    if (mainCamera.UserCommandUpdate(deltaTime)) {
        // should camera auto update descriptor somehow?
        globalDescriptor.SetLocalBufferView(mainCamera);
        globalDescriptor.WriteLocalBuffer();
    }

    const auto commandBuffer = Device->BeginComputeCommandBuffer();

    Device->ResetTimestamps();
    Device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

    auto& nodeSemaphore = nodeReference.exportedSemaphore;
    nodeSemaphore.SyncLocalWaitValue();
    if (nodeSemaphore.localWaitValue != priorNodeSemaphoreWaitValue) {
        priorNodeSemaphoreWaitValue = nodeSemaphore.localWaitValue;
        nodeFramebufferIndex = !nodeFramebufferIndex;

        const auto& nodeFramebuffer = nodeReference.GetExportedFramebuffer(nodeFramebufferIndex);
        nodeFramebuffer.Transition(commandBuffer,
                                   Vulkan::AcquireFromExternalGraphicsAttach,
                                   Vulkan::ToComputeRead);
        computeNodeDescriptor.WriteFramebuffer(nodeFramebuffer);

        const auto& lastUsedNodeDescriptorBuffer = nodeReference.exportedGlobalDescriptor.localBuffer;
        computeNodeDescriptor.localUniformBuffer.view = lastUsedNodeDescriptorBuffer.view;
        computeNodeDescriptor.localUniformBuffer.invView = lastUsedNodeDescriptorBuffer.invView;
        computeNodeDescriptor.localUniformBuffer.viewProj = lastUsedNodeDescriptorBuffer.viewProj;
        computeNodeDescriptor.localUniformBuffer.invViewProj = lastUsedNodeDescriptorBuffer.invViewProj;
        computeNodeDescriptor.WriteLocalBuffer();// does doing a single memcpy to unfiorm actually make it faster?

        nodeReference.SetZCondensedExportedGlobalDescriptorLocalBuffer(mainCamera);
        nodeReference.exportedGlobalDescriptor.WriteLocalBuffer();
    }

    // if (Window::GetUserCommand().debugIncrement != 0) {
    //     computeNodeDescriptor.localUniformBuffer.planeZDepth += Window::GetUserCommand().debugIncrement * 0.1f;
    //     computeNodeDescriptor.WriteLocalBuffer();
    // }

    uint32_t swapIndex;
    swap.Acquire(&swapIndex);
    swap.Transition(commandBuffer,
                    swapIndex,
                    Vulkan::FromComputeSwapPresent,
                    Vulkan::ToComputeWrite);

    const auto& swapImage = swap.GetVkSwapImage(swapIndex);
    const auto& swapImageView = swap.GetVkSwapImageView(swapIndex);
    computeNodeDescriptor.WriteOutputColorImage(swapImageView);

    Vulkan::ComputeNodePipeline::BindDescriptor(commandBuffer, globalDescriptor);
    Vulkan::ComputeNodePipeline::BindDescriptor(commandBuffer, computeNodeDescriptor);

    const auto superSample = 2;
    const auto averagedExtents = outputAveragedAtomicTexture.GetExtents();
    const auto averagedGroupCount = VkExtent2D(averagedExtents.width < Vulkan::ComputeNodePipeline::LocalSize ? 1 : averagedExtents.width / Vulkan::ComputeNodePipeline::LocalSize,
                                               averagedExtents.height < Vulkan::ComputeNodePipeline::LocalSize ? 1 : averagedExtents.height / Vulkan::ComputeNodePipeline::LocalSize);
    computeNodePrePipeline.BindPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, averagedGroupCount.width * superSample, averagedGroupCount.height * superSample, 1);

    const VkImageMemoryBarrier averagedAtomicImageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = outputAveragedAtomicTexture.GetVkImage(),
      .subresourceRange = outputAveragedAtomicTexture.GetSubresourceRange(),
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
                         &averagedAtomicImageMemoryBarrier);
    // const VkBufferMemoryBarrier atomicBufferBarrier{
    //   .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //   .pNext = nullptr,
    //   .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    //   .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    //   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //   .buffer = computeNodeDescriptor.GetStorageBuffer().GetVkBuffer(),
    //   .offset = 0,
    //   .size = sizeof(VkDispatchIndirectCommand),
    // };
    // vkCmdPipelineBarrier(commandBuffer,
    //                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //                      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    //                      0,
    //                      0,
    //                      nullptr,
    //                      1,
    //                      &atomicBufferBarrier,
    //                      0,
    //                      nullptr);

    const auto groupCount = VkExtent2D(Window::GetExtents().width / Vulkan::ComputeNodePipeline::LocalSize,
                                       Window::GetExtents().height / Vulkan::ComputeNodePipeline::LocalSize);
    computeNodePipeline.BindPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);
    // TODO why is indirect slower!?
    // vkCmdDispatchIndirect(commandBuffer, computeNodeDescriptor.GetStorageBuffer().GetVkBuffer(), 0);

    const VkImageMemoryBarrier atomicImageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = outputAtomicTexture.GetVkImage(),
      .subresourceRange = outputAtomicTexture.GetSubresourceRange(),
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

    computeNodePostPipeline.BindPipeline(commandBuffer);
    vkCmdDispatch(commandBuffer, groupCount.width, groupCount.height, 1);

    swap.Transition(commandBuffer,
                    swapIndex,
                    Vulkan::FromComputeWrite,
                    Vulkan::ToComputeSwapPresent);

    Device->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

    vkEndCommandBuffer(commandBuffer);
    Device->SubmitComputeQueueAndPresent(commandBuffer,
                                         swap,
                                         swapIndex,
                                         &semaphore);

    semaphore.Wait();

    const auto timestamps = Device->GetTimestamps();
    const float computeMs = timestamps[1] - timestamps[0];
    // MXC_LOG_NAMED(computeMs);

    return MXC_SUCCESS;
}

MXC_RESULT NodeScene::Init()
{
    MXC_CHK(node.Init());

    // MXC_CHK(m_Swap.Init(Window::extents(), Vulkan::CompositorPipelineType));

    MXC_CHK(standardPipeline.Init());
    MXC_CHK(globalDescriptor.Init(mainCamera,
                                  Window::GetExtents()));

    spherTestTransform.position_ = vec3(0, 0, 0);
    MXC_CHK(sphereTestMesh.InitSphere(0.5f));
    MXC_CHK(sphereTestTexture.InitFromFile("textures/uvgrid.jpg",
                                           Vulkan::Locality::Local));
    MXC_CHK(sphereTestTexture.TransitionInitialImmediate(Vulkan::PipelineType::Graphics));

    MXC_CHK(materialDescriptor.Init(sphereTestTexture));
    MXC_CHK(objectDescriptor.Init(spherTestTransform));

    // spin lock for now until initial ipc message is received
    while (node.ipcFromCompositor().Deque() == 0) {
        MXC_LOG("Node Waiting for IPC");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MXC_CHK(node.pImportedCompositorSemaphore()->SyncLocalWaitValue());
    MXC_CHK(node.pImportedNodeSemaphore()->SyncLocalWaitValue());

    return MXC_SUCCESS;
}

int temp = 0;

MXC_RESULT NodeScene::Loop(const uint32_t& deltaTime)
{
    globalDescriptor.WriteBuffer(node.pImportedGlobalDescriptor()->GetSharedBuffer());

    const auto commandBuffer = Device->BeginGraphicsCommandBuffer();

    const auto& framebuffer = node.framebuffer(framebufferIndex);
    framebuffer.Transition(commandBuffer,
                           Vulkan::AcquireFromExternal,
                           Vulkan::ToGraphicsAttach);

    constexpr auto clearColor = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}};
    Device->BeginRenderPass(framebuffer, clearColor);

    standardPipeline.BindPipeline(commandBuffer);
    standardPipeline.BindDescriptor(commandBuffer, globalDescriptor);
    standardPipeline.BindDescriptor(commandBuffer, materialDescriptor);
    standardPipeline.BindDescriptor(commandBuffer, objectDescriptor);

    sphereTestMesh.RecordRender(commandBuffer);

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
    Device->SubmitGraphicsQueue(node.pImportedNodeSemaphore());
    // k_pDevice->SubmitGraphicsQueueAndPresent(m_Swap,
    //                                          swapIndex,
    //                                          m_Node.ImportedNodeSemaphore());

    node.pImportedNodeSemaphore()->Wait();
    node.pImportedCompositorSemaphore()->localWaitValue += compositorSempahoreStep;
    node.pImportedCompositorSemaphore()->Wait();

    framebufferIndex = !framebufferIndex;

    temp++;
    if (temp == 3) {
        _exit(1);
    }

    return MXC_SUCCESS;
}
