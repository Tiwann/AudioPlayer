#pragma once
#include "Components/Audio/AudioSource.h"
#include "Runtime/Component.h"
#include "Runtime/EntityHandle.h"
#include "Runtime/StaticMesh.h"

using Nova::Ref;
using Nova::StaticMesh;
using Nova::AudioSource;
using Nova::Array;
using Nova::EntityHandle;

class Spectrum final : public Nova::Component
{
public:
    explicit Spectrum(Nova::Entity* owner) : Component(owner, "Spectrum") {}
    ~Spectrum() override = default;

    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnGui() override;


    void InitializeSpectrum();
    void SetAudioSource(AudioSource* audioSource);
    AudioSource* GetAudioSource();

    void SetStaticMesh(Ref<StaticMesh> staticMesh);
    Ref<StaticMesh> GetStaticMesh();

    float GetBarSpace() const;
    void SetBarSpace(float barSpace);
private:
    AudioSource* m_AudioSource = nullptr;
    Ref<StaticMesh> m_StaticMesh = nullptr;
    Array<EntityHandle> m_Entities;
    float m_BarSpace = 1.0f;
    uint32_t m_BarsCount = 20;
    float m_SmoothTime = 0.2f;
};
