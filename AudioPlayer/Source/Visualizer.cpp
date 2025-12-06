#include "Visualizer.h"
#include "Runtime/Application.h"
#include "Runtime/Path.h"
#include "Runtime/Time.h"
#include "Runtime/DesktopWindow.h"
#include "Components/Audio/AudioSource.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/ComputePipeline.h"
#include "Rendering/GraphicsPipeline.h"
#include "Rendering/Sampler.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Rendering/Buffer.h"
#include "Rendering/ShaderBindingSet.h"

#include "Rendering/Vulkan/Texture.h"
#include "Rendering/Vulkan/CommandBuffer.h"
#include <vulkan/vulkan.h>

#include <imgui.h>

using namespace Nova;

static constexpr float FREQ_BUFFER_SIZE = 2048 * sizeof(float);

Visualizer::Visualizer(Entity* owner, const String& name) : Component(owner, name) {}

void Visualizer::OnInit()
{
    Application& application = Application::GetCurrentApplication();
    Ref<Device>& device = application.GetDevice();
    Ref<Window>& window = application.GetWindow();
    const uint32_t width = application.GetWindowWidth();
    const uint32_t height = application.GetWindowHeight();

    const AssetDatabase& assetDatabase = application.GetAssetDatabase();
    m_FullscreenShader = assetDatabase.Get<Shader>("FullscreenShader");

    const ShaderCreateInfo shaderCreateInfo = ShaderCreateInfo()
    .WithTarget(ShaderTarget::SPIRV)
    .WithEntryPoints({ShaderEntryPoint("compute", ShaderStageFlagBits::Compute)})
    .WithModuleInfo({GetObjectName(), Path::GetAssetPath(GetShaderPath())})
    .WithSlang(application.GetSlangSession());
    m_VisualizerShader = device->CreateShader(shaderCreateInfo);

    const SamplerCreateInfo samplerCreateInfo = SamplerCreateInfo()
    .WithFilter(Filter::Linear, Filter::Linear)
    .WithAddressMode(SamplerAddressMode::Repeat);
    m_Sampler = device->CreateSampler(samplerCreateInfo);

    m_StagingBuffer = device->CreateBuffer(BufferUsage::StagingBuffer, FREQ_BUFFER_SIZE);
    m_StagingBuffer->Memset(0, FREQ_BUFFER_SIZE);
    m_VisualizerBuffer = device->CreateBuffer(BufferUsage::StorageBuffer, FREQ_BUFFER_SIZE);

    window->ResizeEvent.BindMember(this, &Visualizer::OnResize);
    SetupFullscreenPipeline(width, height);
    SetupComputePipeline();
}

void Visualizer::OnDestroy()
{
    Application& application = Application::GetCurrentApplication();
    Ref<Device>& device = application.GetDevice();
    device->WaitIdle();

    m_VisualizerPipeline->Destroy();
    m_FullscreenPipeline->Destroy();
    m_VisualizerShader->Destroy();
    m_VisualizerBindingSet->Destroy();
    m_FullscreenBindingSet->Destroy();
    m_Texture->Destroy();
    m_Sampler->Destroy();
    m_VisualizerBuffer->Destroy();
    m_StagingBuffer->Destroy();
}

void Visualizer::OnUpdate(float deltaTime)
{
    const BufferView<float> frequencies = m_AudioSource->GetFrequencies();
    for (size_t freqIndex = 0; freqIndex < frequencies.Count(); ++freqIndex)
    {
        const float targetFreq = frequencies[freqIndex];
        const float currentFreq = m_SmoothedFreqs[freqIndex];
        const float alpha = 1.0f - Math::Exp((-1.0f / m_SmoothTime) * deltaTime);
        const float smoothedFreq = Math::Lerp(currentFreq, targetFreq, alpha);
        m_SmoothedFreqs[freqIndex] = targetFreq > currentFreq ? targetFreq : smoothedFreq;
    }

    m_StagingBuffer->CPUCopy(BufferView<float>(m_SmoothedFreqs, frequencies.Count()), 0);
}

void Visualizer::OnPreRender(CommandBuffer& cmdBuffer)
{
    cmdBuffer.BufferCopy(*m_StagingBuffer, *m_VisualizerBuffer, 0, 0, FREQ_BUFFER_SIZE);
    cmdBuffer.BindComputePipeline(*m_VisualizerPipeline);
    cmdBuffer.BindShaderBindingSet(*m_VisualizerShader, *m_VisualizerBindingSet);


    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.image = m_Texture.As<Vulkan::Texture>()->GetImage();
    barrier.oldLayout = (VkImageLayout)m_Texture.As<Vulkan::Texture>()->GetImageLayout();
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_Texture->GetMips();
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(((Vulkan::CommandBuffer&)cmdBuffer).GetHandle(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    m_Texture.As<Vulkan::Texture>()->SetImageLayout(VK_IMAGE_LAYOUT_GENERAL);

    constexpr uint32_t workGroupSizeX = 16;
    constexpr uint32_t workGroupSizeY = 16;

    const uint32_t numGroupsX = (m_Texture->GetWidth() + workGroupSizeX - 1) / workGroupSizeX;
    const uint32_t numGroupsY = (m_Texture->GetHeight() + workGroupSizeY - 1) / workGroupSizeY;

    m_PushConstants.Seek(Seek::Begin, 0);
    WritePushConstants(m_PushConstants);
    if (m_PushConstants.Size() > 0)
        cmdBuffer.PushConstants(*m_VisualizerShader, ShaderStageFlagBits::Compute, 0, m_PushConstants.Size(), m_PushConstants.Data());
    cmdBuffer.Dispatch(numGroupsX, numGroupsY, 1);

    VkImageMemoryBarrier barrier2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier2.image = m_Texture.As<Vulkan::Texture>()->GetImage();
    barrier2.oldLayout = (VkImageLayout)m_Texture.As<Vulkan::Texture>()->GetImageLayout();
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseMipLevel = 0;
    barrier2.subresourceRange.levelCount = m_Texture->GetMips();
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 1;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(((Vulkan::CommandBuffer&)cmdBuffer).GetHandle(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);
    m_Texture.As<Vulkan::Texture>()->SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Visualizer::OnRender(CommandBuffer& cmdBuffer)
{
    cmdBuffer.BindGraphicsPipeline(*m_FullscreenPipeline);
    cmdBuffer.BindShaderBindingSet(*m_FullscreenShader, *m_FullscreenBindingSet);

    Application& application = Application::GetCurrentApplication();
    const RenderPass* renderPass = application.GetRenderPass();
    cmdBuffer.SetViewport(renderPass->GetOffsetX(), renderPass->GetOffsetY(), renderPass->GetWidth(), renderPass->GetHeight(), 0.0f, 1.0f);
    cmdBuffer.SetScissor(renderPass->GetOffsetX(), renderPass->GetOffsetY(), renderPass->GetWidth(), renderPass->GetHeight());
    cmdBuffer.Draw(6, 1, 0, 0);
}

void Visualizer::OnGui()
{
    ImGui::DragFloat("Smooth Time", &m_SmoothTime, 0.01, 0, 0, "%.2f");
}

void Visualizer::ReloadPipelines()
{
    const Application& application = Application::GetCurrentApplication();
    const uint32_t width = application.GetWindowWidth();
    const uint32_t height = application.GetWindowHeight();
    SetupFullscreenPipeline(width, height);
    SetupComputePipeline();
}

void Visualizer::SetupFullscreenPipeline(uint32_t width, uint32_t height)
{
    Application& application = Application::GetCurrentApplication();
    AssetDatabase& assetDatabase = application.GetAssetDatabase();
    Ref<Device>& device = application.GetDevice();
    device->WaitIdle();

    Ref<Shader> fullscreenShader = assetDatabase.Get<Shader>("FullscreenShader");
    if (!fullscreenShader) return;

    if (m_Texture) m_Texture->Destroy();
    if (m_FullscreenBindingSet) m_FullscreenBindingSet->Destroy();
    if (m_FullscreenPipeline) m_FullscreenPipeline->Destroy();

    const GraphicsPipelineCreateInfo gpCreateInfo = GraphicsPipelineCreateInfo()
    .SetShader(fullscreenShader)
    .SetRenderPass(application.GetRenderPass())
    .SetViewportInfo({0, 0, width, height, 0.0f, 1.0f})
    .SetScissorInfo({0, 0, width, height})
    .SetMultisampleInfo({8});

    m_Texture = device->CreateTexture(TextureUsageFlagBits::Storage | TextureUsageFlagBits::Sampled, width, height, Format::R32G32B32A32_FLOAT);
    m_FullscreenBindingSet = m_FullscreenShader->CreateBindingSet();
    m_FullscreenPipeline = device->CreateGraphicsPipeline(gpCreateInfo);
    m_FullscreenBindingSet->BindCombinedSamplerTexture(0, m_Sampler, m_Texture);
}

void Visualizer::SetupComputePipeline()
{
    Application& application = Application::GetCurrentApplication();
    Ref<Device>& device = application.GetDevice();
    device->WaitIdle();

    if (m_VisualizerPipeline) m_VisualizerPipeline->Destroy();
    if (m_VisualizerBindingSet) m_VisualizerBindingSet->Destroy();

    m_VisualizerPipeline = device->CreateComputePipeline(m_VisualizerShader);
    m_VisualizerBindingSet = m_VisualizerShader->CreateBindingSet();
    m_VisualizerBindingSet->BindTexture(0, m_Texture);
    m_VisualizerBindingSet->BindBuffer(1, m_VisualizerBuffer, 0, FREQ_BUFFER_SIZE);
}


void Visualizer::OnResize(const uint32_t newWidth, const uint32_t newHeight)
{
    SetupFullscreenPipeline(newWidth, newHeight);
    SetupComputePipeline();
}

void Visualizer::SetAudioSource(AudioSource* audioSource)
{
    m_AudioSource = audioSource;
}

AudioSource* Visualizer::GetAudioSource()
{
    return m_AudioSource;
}
