#pragma once
#include "Visualizer.h"

class BarsVisualizer final : public Visualizer
{
public:
    explicit BarsVisualizer(Entity* owner) : Visualizer(owner, "Bars Visualizer") {}
    String GetShaderPath() override;
    void WritePushConstants(Nova::ArrayStream& buffer) override;
};