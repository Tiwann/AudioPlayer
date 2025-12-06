#include "BarsVisualizer.h"
#include "Audio/AudioClip.h"
#include "Audio/AudioFormat.h"
#include "Components/Audio/AudioSource.h"
#include "Runtime/Time.h"

using namespace Nova;

String BarsVisualizer::GetShaderPath()
{
    return "Shaders/BarsVisualizer.slang";
}

void BarsVisualizer::WritePushConstants(ArrayStream& buffer)
{
    const AudioSource* source = GetAudioSource();
    const Ref<AudioClip> clip = source->GetAudioClip();
    const AudioFormat format = clip->GetFormat();
    Ref<FFTAudioNode> fftNode = clip->GetFFTAudioNode();
    const int32_t fftSize = fftNode ? fftNode->GetFFTSize() : 0;
    buffer.Write((float)Time::Get());
    buffer.Write(fftSize);
    buffer.Write(format.channels);
}
