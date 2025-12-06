#pragma once
#include "Containers/String.h"
#include "IO/ArrayStream.h"
#include "Runtime/Component.h"
#include "Runtime/Ref.h"

namespace Nova
{
    class Shader;
    class Texture;
    class Sampler;
    class Material;
    class GraphicsPipeline;
    class ComputePipeline;
    class Buffer;
    class ShaderBindingSet;
    class Entity;
    class AudioSource;
}

using Nova::Component;
using Nova::AudioSource;
using Nova::Ref;
using Nova::Shader;
using Nova::Texture;
using Nova::Material;
using Nova::Entity;
using Nova::Sampler;
using Nova::GraphicsPipeline;
using Nova::ComputePipeline;
using Nova::Buffer;
using Nova::ShaderBindingSet;
using Nova::String;

class Visualizer : public Component
{
public:
    explicit Visualizer(Entity* owner, const String& name);
    virtual String GetShaderPath() = 0;
    virtual void WritePushConstants(Nova::ArrayStream& buffer){};
    virtual void OnResize(uint32_t newWidth, uint32_t newHeight);

    void OnInit() override;
    void OnDestroy() override;
    void OnUpdate(float deltaTime) override;
    void OnPreRender(Nova::CommandBuffer& cmdBuffer) override;
    void OnRender(Nova::CommandBuffer& cmdBuffer) override;
    void OnGui() override;
    void ReloadPipelines();
    void SetAudioSource(AudioSource* audioSource);
    AudioSource* GetAudioSource();
private:
    void SetupFullscreenPipeline(uint32_t width, uint32_t height);
    void SetupComputePipeline();
private:
    AudioSource* m_AudioSource = nullptr;
    Ref<Shader> m_VisualizerShader = nullptr;
    Ref<Texture> m_Texture = nullptr;
    Ref<Sampler> m_Sampler = nullptr;
    Ref<Shader> m_FullscreenShader = nullptr;
    Ref<ShaderBindingSet> m_FullscreenBindingSet = nullptr;
    Ref<ShaderBindingSet> m_VisualizerBindingSet = nullptr;
    Ref<Buffer> m_StagingBuffer = nullptr;
    Ref<Buffer> m_VisualizerBuffer = nullptr;
    Ref<GraphicsPipeline> m_FullscreenPipeline = nullptr;
    Ref<ComputePipeline> m_VisualizerPipeline = nullptr;
    Nova::ArrayStream m_PushConstants;
    float m_SmoothTime = 0.2f;
    float m_SmoothedFreqs[2048]{};
};