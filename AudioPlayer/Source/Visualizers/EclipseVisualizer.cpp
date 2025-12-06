#include "EclipseVisualizer.h"
#include "Runtime/Time.h"
#include <imgui.h>


using namespace Nova;

String EclipseVisualizer::GetShaderPath()
{
    return "Shaders/EclipseVisualizer.slang";
}

void EclipseVisualizer::WritePushConstants(ArrayStream& buffer)
{
    buffer.Write(Time::Get());
    buffer.Write(m_FreqRange);
    buffer.Write(m_Radius);
    buffer.Write(m_Brightness);
    buffer.Write(m_Speed);
}

void EclipseVisualizer::OnGui()
{
    ImGui::DragFloat("Frenquencies Range", &m_FreqRange, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Radius", &m_Radius, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Brightness", &m_Brightness, 0.01f, 0, 0, "%.2f");
    ImGui::DragFloat("Speed", &m_Speed, 0.01f, 0, 0, "%.2f");
}
