#pragma once
#include "Visualizer.h"

class EclipseVisualizer final : public Visualizer
{
public:
    explicit EclipseVisualizer(Entity* owner) : Visualizer(owner, "Eclipse Visualizer") {}

    String GetShaderPath() override;
    void WritePushConstants(Nova::ArrayStream& buffer) override;
    void OnGui() override;
private:
    float m_FreqRange = 63.98f;
    float m_Radius = 0.6;
    float m_Brightness = 0.2;
    float m_Speed = 0.2;
};