#include "EclipseVisualizer.h"
#include "Runtime/Application.h"
#include "Runtime/Path.h"
#include "Runtime/Time.h"
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

#include "Runtime/DesktopWindow.h"

using namespace Nova;


void EclipseVisualizer::OnInit()
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
    .WithModuleInfo({"EclipseVisualizer", Path::GetAssetPath("Shaders/EclipseVisualizer.slang")})
    .WithSlang(application.GetSlangSession());
    m_VisualizerShader = device->CreateShader(shaderCreateInfo);

    const SamplerCreateInfo samplerCreateInfo = SamplerCreateInfo()
    .WithFilter(Filter::Linear, Filter::Linear)
    .WithAddressMode(SamplerAddressMode::Repeat);
    m_Sampler = device->CreateSampler(samplerCreateInfo);

    window->ResizeEvent.BindMember(this, &EclipseVisualizer::OnResize);
    SetupFullscreenPipeline(width, height);
    SetupComputePipeline();
}

void EclipseVisualizer::OnDestroy()
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
}

void EclipseVisualizer::OnUpdate(float deltaTime)
{
    Application& application = Application::GetCurrentApplication();
    auto window = application.GetWindow().As<DesktopWindow>();
    if (window->GetKeyDown(KeyCode::F1))
    {
        SetupFullscreenPipeline(window->GetWidth(), window->GetHeight());
        SetupComputePipeline();
    }
}

void EclipseVisualizer::OnPreRender(CommandBuffer& cmdBuffer)
{
    BufferView<float> frequencies = m_AudioSource->GetFrequencies();
    Array<float> frequenciesSmoothed(frequencies.Data(), frequencies.Count());

    if (!frequencies.IsNullOrEmpty())
        cmdBuffer.UpdateBuffer(*m_VisualizerBuffer, 0, frequencies.Size(), frequencies.Data());

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

    struct MaterialParameters
    {
        float iTime;
        float freqRange = 64.0;
        float radius = 0.6;
        float brightness = 0.2;
        float speed = 0.2;
    } const materialParameters
    {
        (float)Time::Get(),
        m_FreqRange,
        m_Radius,
        m_Brightness,
        m_Speed
    };

    cmdBuffer.PushConstants(*m_VisualizerShader, ShaderStageFlagBits::Compute, 0, sizeof(MaterialParameters), &materialParameters);
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

void EclipseVisualizer::OnRender(CommandBuffer& cmdBuffer)
{
    cmdBuffer.BindGraphicsPipeline(*m_FullscreenPipeline);
    cmdBuffer.BindShaderBindingSet(*m_FullscreenShader, *m_FullscreenBindingSet);

    Application& application = Application::GetCurrentApplication();
    const RenderPass* renderPass = application.GetRenderPass();
    cmdBuffer.SetViewport(renderPass->GetOffsetX(), renderPass->GetOffsetY(), renderPass->GetWidth(), renderPass->GetHeight(), 0.0f, 1.0f);
    cmdBuffer.SetScissor(renderPass->GetOffsetX(), renderPass->GetOffsetY(), renderPass->GetWidth(), renderPass->GetHeight());
    cmdBuffer.Draw(6, 1, 0, 0);
}

void EclipseVisualizer::OnGui()
{
    ImGui::DragFloat("Frenquencies Range", &m_FreqRange, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Radius", &m_Radius, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Brightness", &m_Brightness, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Speed", &m_Speed, 0.01f, 0, 0, "%.2f");
}

void EclipseVisualizer::SetupFullscreenPipeline(uint32_t width, uint32_t height)
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

    m_Texture = device->CreateTexture(TextureUsageFlagBits::Storage | TextureUsageFlagBits::Sampled, 2 * width, 2 * height, Format::R32G32B32A32_FLOAT);
    m_FullscreenBindingSet = m_FullscreenShader->CreateBindingSet();
    m_FullscreenPipeline = device->CreateGraphicsPipeline(gpCreateInfo);
    m_FullscreenBindingSet->BindCombinedSamplerTexture(0, m_Sampler, m_Texture);
}

void EclipseVisualizer::SetupComputePipeline()
{
    Application& application = Application::GetCurrentApplication();
    Ref<Device>& device = application.GetDevice();
    device->WaitIdle();

    if (m_VisualizerPipeline) m_VisualizerPipeline->Destroy();
    if (m_VisualizerBindingSet) m_VisualizerBindingSet->Destroy();
    if (m_VisualizerBuffer) m_VisualizerBuffer->Destroy();

    m_VisualizerPipeline = device->CreateComputePipeline(m_VisualizerShader);
    m_VisualizerBuffer = device->CreateBuffer(BufferUsage::UniformBuffer, 2048 * sizeof(float));
    m_VisualizerBindingSet = m_VisualizerShader->CreateBindingSet();
    m_VisualizerBindingSet->BindTexture(0, m_Texture);
    m_VisualizerBindingSet->BindBuffer(1, m_VisualizerBuffer, 0, 2048 * sizeof(float));
}

void EclipseVisualizer::OnResize(const uint32_t newWidth, const uint32_t newHeight)
{
    SetupFullscreenPipeline(newWidth, newHeight);
    SetupComputePipeline();
}

void EclipseVisualizer::SetAudioSource(AudioSource* audioSource)
{
    m_AudioSource = audioSource;
}
