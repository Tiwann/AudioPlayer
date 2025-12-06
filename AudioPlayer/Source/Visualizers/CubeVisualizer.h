#pragma once
#include "Visualizer.h"

class CubeVisualizer final : public Visualizer
{
public:
    explicit CubeVisualizer(Entity* owner) : Visualizer(owner, "Cube Visualizer") {}

    String GetShaderPath() override;
    void WritePushConstants(Nova::ArrayStream& buffer) override;
private:
    float m_FreqRange = 63.98f;
    float m_Radius = 0.6;
    float m_Brightness = 0.2;
    float m_Speed = 0.2;
};