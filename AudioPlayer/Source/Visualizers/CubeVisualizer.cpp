#include "CubeVisualizer.h"
#include "Runtime/Time.h"

using namespace Nova;

String CubeVisualizer::GetShaderPath()
{
    return "Shaders/CubeVisualizer.slang";
}

void CubeVisualizer::WritePushConstants(ArrayStream& buffer)
{
    buffer.Write(Time::Get());
}