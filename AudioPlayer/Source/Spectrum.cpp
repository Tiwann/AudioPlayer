#include "Spectrum.h"
#include "Components/Transform.h"
#include "Components/Rendering/StaticMeshRenderer.h"
#include "Runtime/Entity.h"
#include "Runtime/Scene.h"
#include <imgui.h>

using namespace Nova;

void Spectrum::OnInit()
{

}

void Spectrum::OnUpdate(float deltaTime)
{
    if (!m_AudioSource) return;
    if (!m_StaticMesh) return;
    if (m_Entities.IsEmpty()) return;

    const BufferView<float> frequencies = m_AudioSource->GetFrequencies();
    if (frequencies.IsNullOrEmpty()) return;

    const uint32_t blockSize = frequencies.Count() / (m_Entities.Count() * 8);

    for (int i = 0; i < m_Entities.Count(); ++i)
    {
        float sum = 0;
        for (int j = 0; j < blockSize; j++)
            sum += frequencies[i * blockSize + j] * frequencies[i * blockSize + j];
        sum /= blockSize;
        sum = Math::Sqrt(sum);

        Transform* transform = m_Entities[i]->GetTransform();
        const float currentHeight = transform->GetScale().y;
        const float targetHeight = 1.0f + 0.6f * sum;
        const float alpha = 1.0f - Math::Exp((-1.0f / m_SmoothTime) * deltaTime);
        const float smoothedHeight = Math::Lerp(currentHeight, targetHeight, alpha);
        const float finalHeight = targetHeight > currentHeight ? targetHeight : smoothedHeight;
        transform->SetScale({1.0, finalHeight, 1.0f});
    }
}

void Spectrum::OnGui()
{
    if (ImGui::DragFloat("Bar Space", &m_BarSpace, 0.01f, 0, 0, "%.2f"))
    {
        InitializeSpectrum();
    }

    if (ImGui::DragInt("Bar Count", (int*)&m_BarsCount, 1, 5, 40))
    {
        InitializeSpectrum();
    }

    if (ImGui::DragFloat("Smooth Time", &m_SmoothTime, 0.01f, 0, 0, "%.2f"))
    {
        InitializeSpectrum();
    }
}

void Spectrum::InitializeSpectrum()
{
    Scene* scene = GetScene();
    for (EntityHandle& entity : m_Entities)
        scene->DestroyEntity(entity);

    Entity* owner = GetOwner();
    for (size_t i = 0; i < m_BarsCount; ++i)
    {
        EntityHandle entity = scene->CreateEntity(StringFormat("SpectrumBar_{}", i));
        Transform* transform = entity->GetTransform();
        transform->SetPosition({i * m_BarSpace, 0.0f, 0.0f});

        StaticMeshRenderer* renderer = entity->AddComponent<StaticMeshRenderer>();
        renderer->SetStaticMesh(m_StaticMesh);
        entity->SetParent(owner);
        m_Entities.Add(entity);
    }
}

void Spectrum::SetAudioSource(AudioSource* audioSource)
{
    m_AudioSource = audioSource;
}

AudioSource* Spectrum::GetAudioSource()
{
    return m_AudioSource;
}

void Spectrum::SetStaticMesh(Ref<StaticMesh> staticMesh)
{
    m_StaticMesh = staticMesh;
    for (EntityHandle& entity : m_Entities)
    {
        StaticMeshRenderer* renderer = entity->GetComponent<StaticMeshRenderer>();
        if (renderer)
        {
            renderer->SetStaticMesh(m_StaticMesh);
        }
    }
}

Ref<StaticMesh> Spectrum::GetStaticMesh()
{
    return m_StaticMesh;
}

float Spectrum::GetBarSpace() const
{
    return m_BarSpace;
}

void Spectrum::SetBarSpace(float barSpace)
{
    m_BarSpace = barSpace;
}
